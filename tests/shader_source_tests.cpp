#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <cassert>
#include <unistd.h>
#include <sys/stat.h>

// Test the shader source extraction directly
#include "render/shader_source.h"

static int testsPassed = 0;
static int testsFailed = 0;

#define TEST(name, expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL: %s\n", name); \
        testsFailed++; \
    } else { \
        fprintf(stdout, "PASS: %s\n", name); \
        testsPassed++; \
    } \
} while(0)

// Helper: create a temp file with given content
static std::string createTempFile(const char* content) {
    char path[] = "/tmp/bh_test_XXXXXX";
    int fd = mkstemp(path);
    if (fd < 0) return "";
    write(fd, content, strlen(content));
    close(fd);
    return std::string(path);
}

// Helper: delete a temp file
static void deleteTempFile(const std::string& path) {
    if (!path.empty()) unlink(path.c_str());
}

void test_readFile() {
    // Test: file not found returns empty string
    std::string result = readFile("/tmp/nonexistent_file_blackhole_test.glsl");
    TEST("readFile: nonexistent file returns empty", result.empty());

    // Test: normal file read
    std::string tmp = createTempFile("hello world");
    if (!tmp.empty()) {
        result = readFile(tmp.c_str());
        TEST("readFile: normal file read", result == "hello world");
        deleteTempFile(tmp);
    }

    // Test: UTF-8 BOM removal
    std::string bom = createTempFile("\xEF\xBB\xBFhello");
    if (!bom.empty()) {
        result = readFile(bom.c_str());
        TEST("readFile: BOM stripped", result == "hello");
        deleteTempFile(bom);
    }

    // Test: CRLF normalization
    std::string crlf = createTempFile("line1\r\nline2\r\n");
    if (!crlf.empty()) {
        result = readFile(crlf.c_str());
        TEST("readFile: CRLF normalized to LF", result == "line1\nline2\n");
        deleteTempFile(crlf);
    }
}

void test_buildFragmentShader_missing_files() {
    // Test: shader resource missing returns false
    // We purposely chdir to /tmp to force file-not-found
    char cwd[4096];
    getcwd(cwd, sizeof(cwd));
    chdir("/tmp");
    std::string out;
    bool ok = buildFragmentShader(out, nullptr);
    chdir(cwd);
    TEST("buildFragmentShader: missing shader files returns false", !ok);
    if (!ok) {
        TEST("buildFragmentShader: missing files returns empty output", out.empty());
    }
}

void test_preset_count_limits() {
    // The header frag_desktop_header.glsl defines MAX_PRESETS = 64.
    // This limits the UI preset count to [1, 64].
    std::string header = readFile("shaders/frag_desktop_header.glsl");
    if (!header.empty()) {
        bool hasDefine = header.find("#define MAX_PRESETS 64") != std::string::npos;
        TEST("preset_limit: header defines MAX_PRESETS = 64", hasDefine);
    } else {
        fprintf(stdout, "SKIP: shaders/frag_desktop_header.glsl not found\n");
    }

    // After buildFragmentShader, the injected demoLook() clamps to MAX_PRESETS
    std::string fragSrc;
    if (buildFragmentShader(fragSrc, nullptr)) {
        bool hasClamp = fragSrc.find("MAX_PRESETS") != std::string::npos;
        TEST("preset_limit: built shader references MAX_PRESETS", hasClamp);
    } else {
        fprintf(stdout, "SKIP: buildFragmentShader failed\n");
    }
}

void test_version_330() {
    // When buildFragmentShader succeeds, the output should contain exactly one #version directive
    std::string fragSrc;
    if (buildFragmentShader(fragSrc, nullptr)) {
        // Count #version occurrences
        int count = 0;
        size_t pos = 0;
        while ((pos = fragSrc.find("#version", pos)) != std::string::npos) {
            count++;
            pos += 8;
        }
        TEST("shader_version: output contains exactly one #version directive", count == 1);
        // Verify it's version 330
        bool has330 = fragSrc.find("#version 330") != std::string::npos;
        TEST("shader_version: version is 330", has330);
    } else {
        fprintf(stdout, "SKIP: buildFragmentShader failed (shader files not in CWD)\n");
    }
}

int main() {
    fprintf(stdout, "=== shader_source tests ===\n");

    test_readFile();
    test_buildFragmentShader_missing_files();
    test_preset_count_limits();
    test_version_330();

    fprintf(stdout, "\nResults: %d passed, %d failed\n", testsPassed, testsFailed);
    return testsFailed > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
