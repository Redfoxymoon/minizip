/* mz_compat.c -- Backwards compatible interface for older versions
   Version 2.7.5, November 13, 2018
   part of the MiniZip project

   Copyright (C) 2010-2018 Nathan Moinvaziri
     https://github.com/nmoinvaz/minizip
   Copyright (C) 1998-2010 Gilles Vollant
     https://www.winimage.com/zLibDll/minizip.html

   This program is distributed under the terms of the same license as zlib.
   See the accompanying LICENSE file for the full text of the license.
*/

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include "mz.h"
#include "mz_os.h"
#include "mz_strm.h"
#include "mz_strm_mem.h"
#include "mz_strm_os.h"
#include "mz_strm_zlib.h"
#include "mz_zip.h"

#include "mz_compat.h"

/***************************************************************************/

typedef struct mz_compat_s {
    void     *stream;
    void     *handle;
    uint64_t entry_index;
    int64_t  entry_pos;
    int64_t  total_out;
} mz_compat;

/***************************************************************************/

static int32_t zipConvertAppendToStreamMode(int append)
{
    int32_t mode = MZ_OPEN_MODE_WRITE;
    switch (append)
    {
    case APPEND_STATUS_CREATE:
        mode |= MZ_OPEN_MODE_CREATE;
        break;
    case APPEND_STATUS_CREATEAFTER:
        mode |= MZ_OPEN_MODE_CREATE | MZ_OPEN_MODE_APPEND;
        break;
    case APPEND_STATUS_ADDINZIP:
        mode |= MZ_OPEN_MODE_READ;
        break;
    }
    return mode;
}

zipFile ZEXPORT zipOpen(const char *path, int append)
{
    zlib_filefunc64_def pzlib = mz_stream_os_get_interface();
    return zipOpen2(path, append, NULL, &pzlib);
}

zipFile ZEXPORT zipOpen64(const void *path, int append)
{
    zlib_filefunc64_def pzlib = mz_stream_os_get_interface();
    return zipOpen2(path, append, NULL, &pzlib);
}

zipFile ZEXPORT zipOpen2(const char *path, int append, const char **globalcomment,
    zlib_filefunc_def *pzlib_filefunc_def)
{
    return zipOpen2_64(path, append, globalcomment, pzlib_filefunc_def);
}

zipFile ZEXPORT zipOpen2_64(const void *path, int append, const char **globalcomment,
    zlib_filefunc64_def *pzlib_filefunc_def)
{
    zipFile zip = NULL;
    int32_t mode = zipConvertAppendToStreamMode(append);
    void *stream = NULL;

    if (pzlib_filefunc_def)
    {
        if (mz_stream_create(&stream, (mz_stream_vtbl *)*pzlib_filefunc_def) == NULL)
            return NULL;
    }
    else
    {
        if (mz_stream_os_create(&stream) == NULL)
            return NULL;
    }

    if (mz_stream_open(stream, path, mode) != MZ_OK)
    {
        mz_stream_delete(&stream);
        return NULL;
    }

    zip = zipOpen_MZ(stream, append, globalcomment);

    if (zip == NULL)
    {
        mz_stream_delete(&stream);
        return NULL;
    }

    return zip;
}

zipFile ZEXPORT zipOpen_MZ(void *stream, int append, const char **globalcomment)
{
    mz_compat *compat = NULL;
    int32_t err = MZ_OK;
    int32_t mode = zipConvertAppendToStreamMode(append);
    void *handle = NULL;

    mz_zip_create(&handle);
    err = mz_zip_open(handle, stream, mode);

    if (err != MZ_OK)
    {
        mz_zip_delete(&handle);
        return NULL;
    }

    if (globalcomment != NULL)
        mz_zip_get_comment(handle, globalcomment);

    compat = (mz_compat *)MZ_ALLOC(sizeof(mz_compat));
    if (compat != NULL)
    {
        compat->handle = handle;
        compat->stream = stream;
    }
    else
    {
        mz_zip_delete(&handle);
    }

    return (zipFile)compat;
}

int ZEXPORT zipOpenNewFileInZip5(zipFile file, const char *filename, const zip_fileinfo *zipfi,
    const void *extrafield_local, uint16_t size_extrafield_local, const void *extrafield_global,
    uint16_t size_extrafield_global, const char *comment, uint16_t compression_method, int level,
    int raw, int windowBits, int memLevel, int strategy, const char *password,
    uint32_t crc_for_crypting,  uint16_t version_madeby, uint16_t flag_base, int zip64)
{
    mz_compat *compat = (mz_compat *)file;
    mz_zip_file file_info;
    uint64_t dos_date = 0;


    MZ_UNUSED(strategy);
    MZ_UNUSED(memLevel);
    MZ_UNUSED(windowBits);
    MZ_UNUSED(size_extrafield_local);
    MZ_UNUSED(extrafield_local);
    MZ_UNUSED(crc_for_crypting);

    if (compat == NULL)
        return ZIP_PARAMERROR;

    memset(&file_info, 0, sizeof(file_info));

    if (zipfi != NULL)
    {
        if (zipfi->dosDate != 0)
            dos_date = zipfi->dosDate;
        else
            dos_date = mz_zip_tm_to_dosdate(&zipfi->tmz_date);

        file_info.modified_date = mz_zip_dosdate_to_time_t(dos_date);
        file_info.external_fa = zipfi->external_fa;
        file_info.internal_fa = zipfi->internal_fa;
    }

    if (filename == NULL)
        filename = "-";

    file_info.compression_method = compression_method;
    file_info.filename = filename;
    /* file_info.extrafield_local = extrafield_local; */
    /* file_info.extrafield_local_size = size_extrafield_local; */
    file_info.extrafield = extrafield_global;
    file_info.extrafield_size = size_extrafield_global;
    file_info.version_madeby = version_madeby;
    file_info.comment = comment;
    file_info.flag = flag_base;
    if (zip64)
        file_info.zip64 = MZ_ZIP64_FORCE;
    else
        file_info.zip64 = MZ_ZIP64_DISABLE;
#ifdef HAVE_AES
    if ((password != NULL) || (raw && (file_info.flag & MZ_ZIP_FLAG_ENCRYPTED)))
        file_info.aes_version = MZ_AES_VERSION;
#endif

    return mz_zip_entry_write_open(compat->handle, &file_info, (int16_t)level, (uint8_t)raw, password);
}

int ZEXPORT zipOpenNewFileInZip4_64(zipFile file, const char *filename, const zip_fileinfo *zipfi,
    const void *extrafield_local, uint16_t size_extrafield_local, const void *extrafield_global,
    uint16_t size_extrafield_global, const char *comment, uint16_t compression_method, int level,
    int raw, int windowBits, int memLevel,   int strategy, const char *password,
    uint32_t crc_for_crypting, uint16_t version_madeby, uint16_t flag_base, int zip64)
{
    return zipOpenNewFileInZip5(file, filename, zipfi, extrafield_local, size_extrafield_local,
        extrafield_global, size_extrafield_global, comment, compression_method, level, raw, windowBits,
        memLevel, strategy, password, crc_for_crypting, version_madeby, flag_base, zip64);
}

int ZEXPORT zipOpenNewFileInZip4(zipFile file, const char *filename, const zip_fileinfo *zipfi,
    const void *extrafield_local, uint16_t size_extrafield_local, const void *extrafield_global,
    uint16_t size_extrafield_global, const char *comment, uint16_t compression_method, int level,
    int raw, int windowBits, int memLevel, int strategy, const char *password,
    uint32_t crc_for_crypting, uint16_t version_madeby, uint16_t flag_base)
{
    return zipOpenNewFileInZip4_64(file, filename, zipfi, extrafield_local, size_extrafield_local,
        extrafield_global, size_extrafield_global, comment, compression_method, level, raw, windowBits,
        memLevel, strategy, password, crc_for_crypting, version_madeby, flag_base, 0);
}

int ZEXPORT zipOpenNewFileInZip3(zipFile file, const char *filename, const zip_fileinfo *zipfi,
    const void *extrafield_local, uint16_t size_extrafield_local, const void *extrafield_global,
    uint16_t size_extrafield_global, const char *comment, uint16_t compression_method, int level,
    int raw, int windowBits, int memLevel, int strategy, const char *password,
    uint32_t crc_for_crypting)
{
    return zipOpenNewFileInZip4_64(file, filename, zipfi, extrafield_local, size_extrafield_local,
        extrafield_global, size_extrafield_global, comment, compression_method, level, raw, windowBits,
        memLevel, strategy, password, crc_for_crypting, MZ_VERSION_MADEBY, 0, 0);
}

int ZEXPORT zipOpenNewFileInZip3_64(zipFile file, const char *filename, const zip_fileinfo *zipfi,
    const void *extrafield_local, uint16_t size_extrafield_local, const void *extrafield_global,
    uint16_t size_extrafield_global, const char *comment, uint16_t compression_method, int level,
    int raw, int windowBits, int memLevel, int strategy, const char *password,
    uint32_t crc_for_crypting, int zip64)
{
    return zipOpenNewFileInZip4_64(file, filename, zipfi, extrafield_local, size_extrafield_local,
        extrafield_global, size_extrafield_global, comment, compression_method, level, raw, windowBits,
        memLevel, strategy, password, crc_for_crypting, MZ_VERSION_MADEBY, 0, zip64);
}

int ZEXPORT zipWriteInFileInZip(zipFile file, const void *buf, uint32_t len)
{
    mz_compat *compat = (mz_compat *)file;
    int32_t written = 0;
    if (compat == NULL || len >= INT32_MAX)
        return ZIP_PARAMERROR;
    written = mz_zip_entry_write(compat->handle, buf, (int32_t)len);
    if ((written < 0) || ((uint32_t)written != len))
        return ZIP_ERRNO;
    return ZIP_OK;
}

int ZEXPORT zipCloseFileInZipRaw(zipFile file, uint32_t uncompressed_size, uint32_t crc32)
{
    return zipCloseFileInZipRaw64(file, uncompressed_size, crc32);
}

int ZEXPORT zipCloseFileInZipRaw64(zipFile file, int64_t uncompressed_size, uint32_t crc32)
{
    mz_compat *compat = (mz_compat *)file;
    if (compat == NULL)
        return ZIP_PARAMERROR;
    return mz_zip_entry_close_raw(compat->handle, uncompressed_size, crc32);
}

int ZEXPORT zipCloseFileInZip(zipFile file)
{
    return zipCloseFileInZip64(file);
}

int ZEXPORT zipCloseFileInZip64(zipFile file)
{
    mz_compat *compat = (mz_compat *)file;
    if (compat == NULL)
        return ZIP_PARAMERROR;
    return mz_zip_entry_close(compat->handle);
}

int ZEXPORT zipClose(zipFile file, const char *global_comment)
{
    return zipClose_64(file, global_comment);
}

int ZEXPORT zipClose_64(zipFile file, const char *global_comment)
{
    return zipClose2_64(file, global_comment, MZ_VERSION_MADEBY);
}

int ZEXPORT zipClose2_64(zipFile file, const char *global_comment, uint16_t version_madeby)
{
    mz_compat *compat = (mz_compat *)file;
    int32_t err = MZ_OK;

    if (compat->handle != NULL)
        err = zipClose2_MZ(file, global_comment, version_madeby);

    if (compat->stream != NULL)
    {
        mz_stream_close(compat->stream);
        mz_stream_delete(&compat->stream);
    }

    MZ_FREE(compat);

    return err;
}

/* Only closes the zip handle, does not close the stream */
int ZEXPORT zipClose_MZ(zipFile file, const char *global_comment)
{
    return zipClose2_MZ(file, global_comment, MZ_VERSION_MADEBY);
}

/* Only closes the zip handle, does not close the stream */
int ZEXPORT zipClose2_MZ(zipFile file, const char *global_comment, uint16_t version_madeby)
{
    mz_compat *compat = (mz_compat *)file;
    int32_t err = MZ_OK;

    if (compat == NULL)
        return ZIP_PARAMERROR;
    if (compat->handle == NULL)
        return err;

    if (global_comment != NULL)
        mz_zip_set_comment(compat->handle, global_comment);

    mz_zip_set_version_madeby(compat->handle, version_madeby);
    err = mz_zip_close(compat->handle);
    mz_zip_delete(&compat->handle);

    return err;
}

void* ZEXPORT zipGetStream(zipFile file)
{
    mz_compat *compat = (mz_compat *)file;
    if (compat == NULL)
        return NULL;
    return (void *)compat->stream;
}

/***************************************************************************/

unzFile ZEXPORT unzOpen(const char *path)
{
    return unzOpen64(path);
}

unzFile ZEXPORT unzOpen64(const void *path)
{
    zlib_filefunc64_def pzlib = mz_stream_os_get_interface();
    return unzOpen2(path, &pzlib);
}

unzFile ZEXPORT unzOpen2(const char *path, zlib_filefunc_def *pzlib_filefunc_def)
{
    return unzOpen2_64(path, pzlib_filefunc_def);
}

unzFile ZEXPORT unzOpen2_64(const void *path, zlib_filefunc64_def *pzlib_filefunc_def)
{
    unzFile unz = NULL;
    void *stream = NULL;

    if (pzlib_filefunc_def)
    {
        if (mz_stream_create(&stream, (mz_stream_vtbl *)*pzlib_filefunc_def) == NULL)
            return NULL;
    }
    else
    {
        if (mz_stream_os_create(&stream) == NULL)
            return NULL;
    }

    if (mz_stream_open(stream, path, MZ_OPEN_MODE_READ) != MZ_OK)
    {
        mz_stream_delete(&stream);
        return NULL;
    }

    unz = unzOpen_MZ(stream);
    if (unz == NULL)
    {
        mz_stream_delete(&stream);
        return NULL;
    }
    return unz;
}

unzFile ZEXPORT unzOpen_MZ(void *stream)
{
    mz_compat *compat = NULL;
    int32_t err = MZ_OK;
    void *handle = NULL;

    mz_zip_create(&handle);
    err = mz_zip_open(handle, stream, MZ_OPEN_MODE_READ);

    if (err != MZ_OK)
    {
        mz_zip_delete(&handle);
        return NULL;
    }

    compat = (mz_compat *)MZ_ALLOC(sizeof(mz_compat));
    if (compat != NULL)
    {
        compat->handle = handle;
        compat->stream = stream;

        mz_zip_goto_first_entry(compat->handle);
    }
    else
    {
        mz_zip_delete(&handle);
    }

    return (unzFile)compat;
}

int ZEXPORT unzClose(unzFile file)
{
    mz_compat *compat = (mz_compat *)file;
    int32_t err = MZ_OK;

    if (compat == NULL)
        return UNZ_PARAMERROR;

    if (compat->handle != NULL)
        err = unzClose_MZ(file);

    if (compat->stream != NULL)
    {
        mz_stream_close(compat->stream);
        mz_stream_delete(&compat->stream);
    }

    MZ_FREE(compat);

    return err;
}

/* Only closes the zip handle, does not close the stream */
int ZEXPORT unzClose_MZ(unzFile file)
{
    mz_compat *compat = (mz_compat *)file;
    int32_t err = MZ_OK;

    if (compat == NULL)
        return UNZ_PARAMERROR;

    err = mz_zip_close(compat->handle);
    mz_zip_delete(&compat->handle);

    return err;
}

int ZEXPORT unzGetGlobalInfo(unzFile file, unz_global_info* pglobal_info32)
{
    mz_compat *compat = (mz_compat *)file;
    unz_global_info64 global_info64;
    int32_t err = MZ_OK;

    memset(pglobal_info32, 0, sizeof(unz_global_info));
    if (compat == NULL)
        return UNZ_PARAMERROR;

    err = unzGetGlobalInfo64(file, &global_info64);
    if (err == MZ_OK)
    {
        pglobal_info32->number_entry = (uint32_t)global_info64.number_entry;
        pglobal_info32->size_comment = global_info64.size_comment;
        pglobal_info32->number_disk_with_CD = global_info64.number_disk_with_CD;
    }
    return err;
}

int ZEXPORT unzGetGlobalInfo64(unzFile file, unz_global_info64 *pglobal_info)
{
    mz_compat *compat = (mz_compat *)file;
    const char *comment_ptr = NULL;
    int32_t err = MZ_OK;

    memset(pglobal_info, 0, sizeof(unz_global_info64));
    if (compat == NULL)
        return UNZ_PARAMERROR;
    err = mz_zip_get_comment(compat->handle, &comment_ptr);
    if (err == MZ_OK)
        pglobal_info->size_comment = (uint16_t)strlen(comment_ptr);
    if ((err == MZ_OK) || (err == MZ_EXIST_ERROR))
        err = mz_zip_get_number_entry(compat->handle, &pglobal_info->number_entry);
    if (err == MZ_OK)
        err = mz_zip_get_disk_number_with_cd(compat->handle, &pglobal_info->number_disk_with_CD);
    return err;
}

int ZEXPORT unzGetGlobalComment(unzFile file, char *comment, uint16_t comment_size)
{
    mz_compat *compat = (mz_compat *)file;
    const char *comment_ptr = NULL;
    int32_t err = MZ_OK;

    if (comment == NULL || comment_size == 0)
        return UNZ_PARAMERROR;
    err = mz_zip_get_comment(compat->handle, &comment_ptr);
    if (err == MZ_OK)
    {
        strncpy(comment, comment_ptr, comment_size - 1);
        comment[comment_size - 1] = 0;
    }
    return err;
}

int ZEXPORT unzOpenCurrentFile3(unzFile file, int *method, int *level, int raw, const char *password)
{
    mz_compat *compat = (mz_compat *)file;
    mz_zip_file *file_info = NULL;
    int32_t err = MZ_OK;
    void *stream = NULL;

    if (compat == NULL)
        return UNZ_PARAMERROR;
    if (method != NULL)
        *method = 0;
    if (level != NULL)
        *level = 0;

    compat->total_out = 0;
    err = mz_zip_entry_read_open(compat->handle, (uint8_t)raw, password);
    if (err == MZ_OK)
        err = mz_zip_entry_get_info(compat->handle, &file_info);
    if (err == MZ_OK)
    {
        if (method != NULL)
        {
            *method = file_info->compression_method;
        }

        if (level != NULL)
        {
            *level = 6;
            switch (file_info->flag & 0x06)
            {
            case MZ_ZIP_FLAG_DEFLATE_SUPER_FAST:
                *level = 1;
                break;
            case MZ_ZIP_FLAG_DEFLATE_FAST:
                *level = 2;
                break;
            case MZ_ZIP_FLAG_DEFLATE_MAX:
                *level = 9;
                break;
            }
        }
    }
    if (err == MZ_OK)
        err = mz_zip_get_stream(compat->handle, &stream);
    if (err == MZ_OK)
        compat->entry_pos = mz_stream_tell(stream);
    return err;
}

int ZEXPORT unzOpenCurrentFile(unzFile file)
{
    return unzOpenCurrentFile3(file, NULL, NULL, 0, NULL);
}

int ZEXPORT unzOpenCurrentFilePassword(unzFile file, const char *password)
{
    return unzOpenCurrentFile3(file, NULL, NULL, 0, password);
}

int ZEXPORT unzOpenCurrentFile2(unzFile file, int *method, int *level, int raw)
{
    return unzOpenCurrentFile3(file, method, level, raw, NULL);
}

int ZEXPORT unzReadCurrentFile(unzFile file, void *buf, uint32_t len)
{
    mz_compat *compat = (mz_compat *)file;
    int32_t err = MZ_OK;
    if (compat == NULL || len >= INT32_MAX)
        return UNZ_PARAMERROR;
    err = mz_zip_entry_read(compat->handle, buf, (int32_t)len);
    if (err > 0)
        compat->total_out += (uint32_t)err;
    return err;
}

int ZEXPORT unzCloseCurrentFile(unzFile file)
{
    mz_compat *compat = (mz_compat *)file;
    int32_t err = MZ_OK;
    if (compat == NULL)
        return UNZ_PARAMERROR;
    err = mz_zip_entry_close(compat->handle);
    return err;
}

int ZEXPORT unzGetCurrentFileInfo(unzFile file, unz_file_info *pfile_info, char *filename,
    uint16_t filename_size, void *extrafield, uint16_t extrafield_size, char *comment, uint16_t comment_size)
{
    mz_compat *compat = (mz_compat *)file;
    mz_zip_file *file_info = NULL;
    uint16_t bytes_to_copy = 0;
    int32_t err = MZ_OK;

    if (compat == NULL)
        return UNZ_PARAMERROR;

    err = mz_zip_entry_get_info(compat->handle, &file_info);

    if ((err == MZ_OK) && (pfile_info != NULL))
    {
        pfile_info->version = file_info->version_madeby;
        pfile_info->version_needed = file_info->version_needed;
        pfile_info->flag = file_info->flag;
        pfile_info->compression_method = file_info->compression_method;
        pfile_info->dosDate = mz_zip_time_t_to_dos_date(file_info->modified_date);
        mz_zip_time_t_to_tm(file_info->modified_date, &pfile_info->tmu_date);
        pfile_info->tmu_date.tm_year += 1900;
        pfile_info->crc = file_info->crc;

        pfile_info->size_filename = file_info->filename_size;
        pfile_info->size_file_extra = file_info->extrafield_size;
        pfile_info->size_file_comment = file_info->comment_size;

        pfile_info->disk_num_start = (uint16_t)file_info->disk_number;
        pfile_info->internal_fa = file_info->internal_fa;
        pfile_info->external_fa = file_info->external_fa;

        pfile_info->compressed_size = (uint32_t)file_info->compressed_size;
        pfile_info->uncompressed_size = (uint32_t)file_info->uncompressed_size;

        if (filename_size > 0 && filename != NULL && file_info->filename != NULL)
        {
            bytes_to_copy = filename_size;
            if (bytes_to_copy > file_info->filename_size)
                bytes_to_copy = file_info->filename_size;
            memcpy(filename, file_info->filename, bytes_to_copy);
            if (bytes_to_copy < filename_size)
                filename[bytes_to_copy] = 0;
        }
        if (extrafield_size > 0 && extrafield != NULL)
        {
            bytes_to_copy = extrafield_size;
            if (bytes_to_copy > file_info->extrafield_size)
                bytes_to_copy = file_info->extrafield_size;
            memcpy(extrafield, file_info->extrafield, bytes_to_copy);
        }
        if (comment_size > 0 && comment != NULL && file_info->comment != NULL)
        {
            bytes_to_copy = comment_size;
            if (bytes_to_copy > file_info->comment_size)
                bytes_to_copy = file_info->comment_size;
            memcpy(comment, file_info->comment, bytes_to_copy);
            if (bytes_to_copy < comment_size)
                comment[bytes_to_copy] = 0;
        }
    }
    return err;
}

int ZEXPORT unzGetCurrentFileInfo64(unzFile file, unz_file_info64 * pfile_info, char *filename,
    uint16_t filename_size, void *extrafield, uint16_t extrafield_size, char *comment, uint16_t comment_size)
{
    mz_compat *compat = (mz_compat *)file;
    mz_zip_file *file_info = NULL;
    uint16_t bytes_to_copy = 0;
    int32_t err = MZ_OK;

    if (compat == NULL)
        return UNZ_PARAMERROR;

    err = mz_zip_entry_get_info(compat->handle, &file_info);

    if ((err == MZ_OK) && (pfile_info != NULL))
    {
        pfile_info->version = file_info->version_madeby;
        pfile_info->version_needed = file_info->version_needed;
        pfile_info->flag = file_info->flag;
        pfile_info->compression_method = file_info->compression_method;
        pfile_info->dosDate = mz_zip_time_t_to_dos_date(file_info->modified_date);
        mz_zip_time_t_to_tm(file_info->modified_date, &pfile_info->tmu_date);
        pfile_info->tmu_date.tm_year += 1900;
        pfile_info->crc = file_info->crc;

        pfile_info->size_filename = file_info->filename_size;
        pfile_info->size_file_extra = file_info->extrafield_size;
        pfile_info->size_file_comment = file_info->comment_size;

        pfile_info->disk_num_start = file_info->disk_number;
        pfile_info->internal_fa = file_info->internal_fa;
        pfile_info->external_fa = file_info->external_fa;

        pfile_info->compressed_size = (uint64_t)file_info->compressed_size;
        pfile_info->uncompressed_size = (uint64_t)file_info->uncompressed_size;

        if (filename_size > 0 && filename != NULL && file_info->filename != NULL)
        {
            bytes_to_copy = filename_size;
            if (bytes_to_copy > file_info->filename_size)
                bytes_to_copy = file_info->filename_size;
            memcpy(filename, file_info->filename, bytes_to_copy);
            if (bytes_to_copy < filename_size)
                filename[bytes_to_copy] = 0;
        }

        if (extrafield_size > 0 && extrafield != NULL)
        {
            bytes_to_copy = extrafield_size;
            if (bytes_to_copy > file_info->extrafield_size)
                bytes_to_copy = file_info->extrafield_size;
            memcpy(extrafield, file_info->extrafield, bytes_to_copy);
        }

        if (comment_size > 0 && comment != NULL && file_info->comment != NULL)
        {
            bytes_to_copy = comment_size;
            if (bytes_to_copy > file_info->comment_size)
                bytes_to_copy = file_info->comment_size;
            memcpy(comment, file_info->comment, bytes_to_copy);
            if (bytes_to_copy < comment_size)
                comment[bytes_to_copy] = 0;
        }
    }
    return err;
}

int ZEXPORT unzGoToFirstFile(unzFile file)
{
    mz_compat *compat = (mz_compat *)file;
    if (compat == NULL)
        return UNZ_PARAMERROR;
    compat->entry_index = 0;
    return mz_zip_goto_first_entry(compat->handle);
}

int ZEXPORT unzGoToNextFile(unzFile file)
{
    mz_compat *compat = (mz_compat *)file;
    int32_t err = MZ_OK;
    if (compat == NULL)
        return UNZ_PARAMERROR;
    err = mz_zip_goto_next_entry(compat->handle);
    if (err != MZ_END_OF_LIST)
        compat->entry_index += 1;
    return err;
}

int ZEXPORT unzLocateFile(unzFile file, const char *filename, unzFileNameComparer filename_compare_func)
{
    mz_compat *compat = (mz_compat *)file;
    mz_zip_file *file_info = NULL;
    uint64_t preserve_index = 0;
    int32_t err = MZ_OK;
    int32_t result = 0;

    if (compat == NULL)
        return UNZ_PARAMERROR;

    preserve_index = compat->entry_index;

    err = mz_zip_goto_first_entry(compat->handle);
    while (err == MZ_OK)
    {
        err = mz_zip_entry_get_info(compat->handle, &file_info);
        if (err != MZ_OK)
            break;

        if (filename_compare_func != NULL)
            result = filename_compare_func(file, filename, file_info->filename);
        else
            result = strcmp(filename, file_info->filename);

        if (result == 0)
            return MZ_OK;

        err = mz_zip_goto_next_entry(compat->handle);
    }

    compat->entry_index = preserve_index;
    return err;
}

/***************************************************************************/

int ZEXPORT unzGetFilePos(unzFile file, unz_file_pos *file_pos)
{
    mz_compat *compat = (mz_compat *)file;
    int32_t offset = 0;
    
    if (compat == NULL || file_pos == NULL)
        return UNZ_PARAMERROR;
    
    offset = unzGetOffset(file);
    if (offset < 0)
        return offset;
    
    file_pos->pos_in_zip_directory = (uint32_t)offset;
    file_pos->num_of_file = (uint32_t)compat->entry_index;
    return MZ_OK;
}

int ZEXPORT unzGoToFilePos(unzFile file, unz_file_pos *file_pos)
{
    mz_compat *compat = (mz_compat *)file;
    unz64_file_pos file_pos64;

    if (compat == NULL || file_pos == NULL)
        return UNZ_PARAMERROR;

    file_pos64.pos_in_zip_directory = file_pos->pos_in_zip_directory;
    file_pos64.num_of_file = file_pos->num_of_file;

    return unzGoToFilePos64(file, &file_pos64);
}

int ZEXPORT unzGetFilePos64(unzFile file, unz64_file_pos *file_pos)
{
    mz_compat *compat = (mz_compat *)file;
    int64_t offset = 0;
    
    if (compat == NULL || file_pos == NULL)
        return UNZ_PARAMERROR;
    
    offset = unzGetOffset64(file);
    if (offset < 0)
        return (int)offset;
    
    file_pos->pos_in_zip_directory = offset;
    file_pos->num_of_file = compat->entry_index;
    return UNZ_OK;
}

int ZEXPORT unzGoToFilePos64(unzFile file, const unz64_file_pos *file_pos)
{
    mz_compat *compat = (mz_compat *)file;
    int32_t err = MZ_OK;

    if (compat == NULL || file_pos == NULL)
        return UNZ_PARAMERROR;

    err = mz_zip_goto_entry(compat->handle, file_pos->pos_in_zip_directory);
    if (err == MZ_OK)
        compat->entry_index = file_pos->num_of_file;
    return err;
}

int32_t ZEXPORT unzGetOffset(unzFile file)
{
    return (int32_t)unzGetOffset64(file);
}

int64_t ZEXPORT unzGetOffset64(unzFile file)
{
    mz_compat *compat = (mz_compat *)file;
    if (compat == NULL)
        return UNZ_PARAMERROR;
    return mz_zip_get_entry(compat->handle);
}

int ZEXPORT unzSetOffset(unzFile file, uint32_t pos)
{
    return unzSetOffset64(file, pos);
}

int ZEXPORT unzSetOffset64(unzFile file, int64_t pos)
{
    mz_compat *compat = (mz_compat *)file;
    if (compat == NULL)
        return UNZ_PARAMERROR;
    return (int)mz_zip_goto_entry(compat->handle, pos);
}

int ZEXPORT unzGetLocalExtrafield(unzFile file, void *buf, unsigned int len)
{
    mz_compat *compat = (mz_compat *)file;
    mz_zip_file *file_info = NULL;
    int32_t err = MZ_OK;
    int32_t bytes_to_copy = 0;

    if (compat == NULL || buf == NULL || len >= INT32_MAX)
        return UNZ_PARAMERROR;
    
    err = mz_zip_entry_get_local_info(compat->handle, &file_info);
    if (err != MZ_OK)
        return err;
    
    bytes_to_copy = (int32_t)len;
    if (bytes_to_copy > file_info->extrafield_size)
        bytes_to_copy = file_info->extrafield_size;

    memcpy(buf, file_info->extrafield, bytes_to_copy);
    return MZ_OK;
}

int32_t ZEXPORT unzTell(unzFile file)
{
    mz_compat *compat = (mz_compat *)file;
    if (compat == NULL)
        return UNZ_PARAMERROR;
    return (int32_t)compat->total_out;
}

int64_t ZEXPORT unzTell64(unzFile file)
{
    mz_compat *compat = (mz_compat *)file;
    if (compat == NULL)
        return UNZ_PARAMERROR;
    return (int64_t)compat->total_out;
}

int ZEXPORT unzSeek(unzFile file, int32_t offset, int origin)
{
    return unzSeek64(file, offset, origin);
}

int ZEXPORT unzSeek64(unzFile file, int64_t offset, int origin)
{
    mz_compat *compat = (mz_compat *)file;
    mz_zip_file *file_info = NULL;
    int64_t position = 0;
    int32_t err = MZ_OK;
    void *stream = NULL;

    if (compat == NULL)
        return UNZ_PARAMERROR;
    err = mz_zip_entry_get_info(compat->handle, &file_info);
    if (err != MZ_OK)
        return err;
    if (file_info->compression_method != MZ_COMPRESS_METHOD_STORE)
        return UNZ_ERRNO;

    if (origin == SEEK_SET)
        position = offset;
    else if (origin == SEEK_CUR)
        position = compat->total_out + offset;
    else if (origin == SEEK_END)
        position = (int64_t)file_info->compressed_size + offset;
    else
        return UNZ_PARAMERROR;

    if (position > (int64_t)file_info->compressed_size)
        return UNZ_PARAMERROR;

    err = mz_zip_get_stream(compat->handle, &stream);
    if (err == MZ_OK)
        err = mz_stream_seek(stream, compat->entry_pos + position, SEEK_SET);
    if (err == MZ_OK)
        compat->total_out = position;
    return err;
}

int ZEXPORT unzEndOfFile(unzFile file)
{
    mz_compat *compat = (mz_compat *)file;
    mz_zip_file *file_info = NULL;
    int32_t err = MZ_OK;

    if (compat == NULL)
        return UNZ_PARAMERROR;
    err = mz_zip_entry_get_info(compat->handle, &file_info);
    if (err != MZ_OK)
        return err;
    if (compat->total_out == (int64_t)file_info->uncompressed_size)
        return 1;
    return 0;
}

void* ZEXPORT unzGetStream(unzFile file)
{
    mz_compat *compat = (mz_compat *)file;
    if (compat == NULL)
        return NULL;
    return (void *)compat->stream;
}

/***************************************************************************/

void fill_fopen_filefunc(zlib_filefunc_def *pzlib_filefunc_def)
{
    if (pzlib_filefunc_def != NULL)
        *pzlib_filefunc_def = mz_stream_os_get_interface();
}

void fill_fopen64_filefunc(zlib_filefunc64_def *pzlib_filefunc_def)
{
    if (pzlib_filefunc_def != NULL)
        *pzlib_filefunc_def = mz_stream_os_get_interface();
}

void fill_win32_filefunc(zlib_filefunc_def *pzlib_filefunc_def)
{
    if (pzlib_filefunc_def != NULL)
        *pzlib_filefunc_def = mz_stream_os_get_interface();
}

void fill_win32_filefunc64(zlib_filefunc64_def *pzlib_filefunc_def)
{
    if (pzlib_filefunc_def != NULL)
        *pzlib_filefunc_def = mz_stream_os_get_interface();
}

void fill_win32_filefunc64A(zlib_filefunc64_def *pzlib_filefunc_def)
{
    if (pzlib_filefunc_def != NULL)
        *pzlib_filefunc_def = mz_stream_os_get_interface();
}

void fill_win32_filefunc64W(zlib_filefunc64_def *pzlib_filefunc_def)
{
    /* NOTE: You should no longer pass in widechar string to open function */
    if (pzlib_filefunc_def != NULL)
        *pzlib_filefunc_def = mz_stream_os_get_interface();
}

void fill_memory_filefunc(zlib_filefunc_def *pzlib_filefunc_def)
{
    if (pzlib_filefunc_def != NULL)
        *pzlib_filefunc_def = mz_stream_mem_get_interface();
}
