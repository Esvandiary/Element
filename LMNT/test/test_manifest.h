#include "CUnit/CUnitCI.h"
#include "lmnt/interpreter.h"
#include "testhelpers.h"
#include <stdio.h>
#include <stdbool.h>

#if !defined(TESTSETUP_INCLUDED)
#error "This file cannot be included without a testsetup header already having been included"
#endif


static archive create_archive_array_with_manifest(const char* name, lmnt_manifest_sections sections, size_t sections_count, ...)
{
    const size_t name_len = strlen(name);
    assert(name_len <= 0xFE);

    size_t manifest_len = sizeof(lmnt_manifest_sections);
    va_list args;
    va_start(args, sections_count);
    for (size_t i = 0; i < sections_count; ++i) {
        /* section = */ va_arg(args, void*);
        manifest_len += va_arg(args, size_t);
    }
    va_end(args);

    const size_t header_len = sizeof(lmnt_archive_header);
    const size_t strings_len = (0x02 + name_len + 1);
    const size_t defs_len = 0x00;
    uint32_t code_len = 0x00;
    const uint32_t code_padding = (4 - ((header_len + strings_len + defs_len + code_len) % 4)) % 4;
    code_len += code_padding;
    uint32_t data_len = 0x04;
    const uint32_t consts_len = 0x00;

    const size_t total_size = header_len + manifest_len + strings_len + defs_len + code_len + data_len + consts_len;
    char* buf = (char*)calloc(total_size, sizeof(char));

    size_t idx = 0;
    const char header[] = {
        'L', 'M', 'N', 'T',
        0x00, 0x00, 0x00, 0x00,
        manifest_len & 0xFF, (manifest_len >> 8) & 0xFF, (manifest_len >> 16) & 0xFF, (manifest_len >> 24) & 0xFF, // manifest length
        strings_len & 0xFF, (strings_len >> 8) & 0xFF, (strings_len >> 16) & 0xFF, (strings_len >> 24) & 0xFF, // strings length
        defs_len & 0xFF, (defs_len >> 8) & 0xFF, (defs_len >> 16) & 0xFF, (defs_len >> 24) & 0xFF, // defs length
        code_len & 0xFF, (code_len >> 8) & 0xFF, (code_len >> 16) & 0xFF, (code_len >> 24) & 0xFF, // code length
        data_len & 0xFF, (data_len >> 8) & 0xFF, (data_len >> 16) & 0xFF, (data_len >> 24) & 0xFF, // data length
        consts_len & 0xFF, (consts_len >> 8) & 0xFF, (consts_len >> 16) & 0xFF, (consts_len >> 24) & 0xFF // constants_length
    };
    memcpy(buf + idx, header, sizeof(header));
    idx += sizeof(header);

    // manifest
    memcpy(buf + idx, &sections, sizeof(lmnt_manifest_sections));
    idx += sizeof(lmnt_manifest_sections);
    va_start(args, sections_count);
    for (size_t i = 0; i < sections_count; ++i) {
        const char* section = (const char*)va_arg(args, void*);
        size_t sec_len = va_arg(args, int);
        memcpy(buf + idx, section, sec_len);
        idx += sec_len;
    }
    va_end(args);

    // strings
    buf[idx] = (name_len + 1) & 0xFF;
    idx += 2;
    memcpy(buf + idx, name, name_len);
    idx += name_len;
    buf[idx++] = '\0';

    // padding
    for (size_t i = 0; i < code_padding; ++i)
        buf[idx++] = 0;
    // data
    buf[idx++] = 0;
    buf[idx++] = 0;
    buf[idx++] = 0;
    buf[idx++] = 0;

    assert(idx == total_size);

    archive a = {buf, total_size};
    return a;
}


static void test_manifest_no_sections(void)
{
    archive a = create_archive_array_with_manifest("test", LMNT_MANIFEST_NONE, 0);

    test_function_data fndata = { NULL, NULL };
    CU_ASSERT_EQUAL_FATAL(lmnt_ictx_load_archive(ctx, a.buf, a.size), LMNT_OK);
    lmnt_validation_result vr;
    CU_ASSERT_EQUAL(lmnt_ictx_prepare_archive(ctx, &vr), LMNT_OK);
    CU_ASSERT_EQUAL(vr, LMNT_VALIDATION_OK);
    delete_archive_array(a);

    TEST_UNLOAD_ARCHIVE(ctx, a, fndata);
}

static void test_manifest_basic(void)
{
    const char section[] = {
        0x00, 0x00, // name
        0x00, 0x00, // major.minor
        0x3D, 0x32, 0xD8, 0x30, // crc32(0x05 0x00 't' 'e' 's' 't' [0x00 x 6]) == 0x30D8323D
    };

    archive a = create_archive_array_with_manifest("test", LMNT_MANIFEST_BASIC, 1,
        section, sizeof(section)
    );

    test_function_data fndata = { NULL, NULL };
    CU_ASSERT_EQUAL_FATAL(lmnt_ictx_load_archive(ctx, a.buf, a.size), LMNT_OK);
    lmnt_validation_result vr;
    CU_ASSERT_EQUAL(lmnt_ictx_prepare_archive(ctx, &vr), LMNT_OK);
    CU_ASSERT_EQUAL(vr, LMNT_VALIDATION_OK);
    delete_archive_array(a);

    TEST_UNLOAD_ARCHIVE(ctx, a, fndata);
}

static void test_manifest_invalid_name(void)
{
    const char section[] = {
        0x05, // invalid string index
        0x00, 0x00, // major.minor
        0x3D, 0x32, 0xD8, 0x30, // crc32
    };
    archive a = create_archive_array_with_manifest("test", LMNT_MANIFEST_BASIC, 1,
        section, sizeof(section)
    );

    test_function_data fndata = { NULL, NULL };
    TEST_LOAD_ARCHIVE_FAILS_VALIDATION(ctx, "test", a, fndata,
        LMNT_ERROR_INVALID_ARCHIVE, LMNT_VERROR_INVALID_MANIFEST);
    delete_archive_array(a);

    TEST_UNLOAD_ARCHIVE(ctx, a, fndata);
}

static void test_manifest_invalid_crc32(void)
{
    const char section[] = {
        0x02, // name
        0x00, 0x00, // major.minor
        0x12, 0x34, 0x56, 0x78, // wrong CRC
    };
    archive a = create_archive_array_with_manifest("test", LMNT_MANIFEST_BASIC, 1,
        section, sizeof(section)
    );

    test_function_data fndata = { NULL, NULL };
    TEST_LOAD_ARCHIVE_FAILS_VALIDATION(ctx, "test", a, fndata,
        LMNT_ERROR_INVALID_ARCHIVE, LMNT_VERROR_INVALID_MANIFEST);
    delete_archive_array(a);

    TEST_UNLOAD_ARCHIVE(ctx, a, fndata);
}



MAKE_REGISTER_SUITE_FUNCTION(manifest,
    CUNIT_CI_TEST(test_manifest_no_sections),
    CUNIT_CI_TEST(test_manifest_basic),
    CUNIT_CI_TEST(test_manifest_invalid_name),
    CUNIT_CI_TEST(test_manifest_invalid_crc32)
);