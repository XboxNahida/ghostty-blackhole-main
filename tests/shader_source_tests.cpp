#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <cassert>
#include <unistd.h>
#include <sys/stat.h>

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

static std::string createTempFile(const char* content) {
    char path[] = "/tmp/bh_test_XXXXXX";
    int fd = mkstemp(path);
    if (fd < 0) return "";
    write(fd, content, strlen(content));
    close(fd);
    return std::string(path);
}

static void deleteTempFile(const std::string& path) {
    if (!path.empty()) unlink(path.c_str());
}

void test_readFile() {
    std::string result = readFile("/tmp/nonexistent_file_blackhole_test.glsl");
    TEST("readFile: nonexistent file returns empty", result.empty());

    std::string tmp = createTempFile("hello world");
    if (!tmp.empty()) {
        result = readFile(tmp.c_str());
        TEST("readFile: normal file read", result == "hello world");
        deleteTempFile(tmp);
    }

    std::string bom = createTempFile("\xEF\xBB\xBFhello");
    if (!bom.empty()) {
        result = readFile(bom.c_str());
        TEST("readFile: BOM stripped", result == "hello");
        deleteTempFile(bom);
    }

    std::string crlf = createTempFile("line1\r\nline2\r\n");
    if (!crlf.empty()) {
        result = readFile(crlf.c_str());
        TEST("readFile: CRLF normalized to LF", result == "line1\nline2\n");
        deleteTempFile(crlf);
    }
}

void test_buildFragmentShader_missing_files() {
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
    std::string header = readFile("shaders/frag_desktop_header.glsl");
    if (!header.empty()) {
        bool hasDefine = header.find("#define MAX_PRESETS 64") != std::string::npos;
        TEST("preset_limit: header defines MAX_PRESETS = 64", hasDefine);
    } else {
        fprintf(stdout, "SKIP: shaders/frag_desktop_header.glsl not found\n");
    }

    std::string fragSrc;
    if (buildFragmentShader(fragSrc, nullptr)) {
        bool hasClamp = fragSrc.find("MAX_PRESETS") != std::string::npos;
        TEST("preset_limit: built shader references MAX_PRESETS", hasClamp);
    } else {
        fprintf(stdout, "SKIP: buildFragmentShader failed\n");
    }
}

void test_version_330() {
    std::string fragSrc;
    if (buildFragmentShader(fragSrc, nullptr)) {
        int count = 0;
        size_t pos = 0;
        while ((pos = fragSrc.find("#version", pos)) != std::string::npos) {
            count++;
            pos += 8;
        }
        TEST("shader_version: output contains exactly one #version directive", count == 1);
        bool has330 = fragSrc.find("#version 330") != std::string::npos;
        TEST("shader_version: version is 330", has330);
    } else {
        fprintf(stdout, "SKIP: buildFragmentShader failed (shader files not in CWD)\n");
    }
}

void test_applyPatches() {
    // Test: critical patch succeeds
    {
        std::string s = "hello foo world";
        ShaderPatch patches[] = {
            {"foo", "bar", "replace foo with bar", true}
        };
        bool ok = applyPatches(s, patches, 1, nullptr);
        TEST("applyPatches: critical anchor found succeeds", ok && s == "hello bar world");
    }

    // Test: critical patch missing returns false
    {
        std::string s = "hello world";
        ShaderPatch patches[] = {
            {"foo", "bar", "replace foo with bar (missing)", true}
        };
        bool ok = applyPatches(s, patches, 1, nullptr);
        TEST("applyPatches: critical anchor missing returns false", !ok);
        TEST("applyPatches: body unchanged when critical missing", s == "hello world");
    }

    // Test: optional patch missing does not fail
    {
        std::string s = "hello world";
        ShaderPatch patches[] = {
            {"foo", "bar", "replace foo with bar (optional)", false}
        };
        bool ok = applyPatches(s, patches, 1, nullptr);
        TEST("applyPatches: optional anchor missing returns true", ok);
        TEST("applyPatches: body unchanged when optional missing", s == "hello world");
    }

    // Test: mixed critical/optional — both found
    {
        std::string s = "aaa bbb";
        ShaderPatch patches[] = {
            {"aaa", "xxx", "optional aaa", false},
            {"bbb", "yyy", "critical bbb", true}
        };
        bool ok = applyPatches(s, patches, 2, nullptr);
        TEST("applyPatches: mixed all found returns true", ok);
        TEST("applyPatches: mixed all found — body updated", s == "xxx yyy");
    }

    // Test: mixed — critical missing, optional found
    {
        std::string s = "aaa bbb";
        ShaderPatch patches[] = {
            {"aaa", "xxx", "optional aaa", false},
            {"ccc", "yyy", "critical ccc missing", true}
        };
        bool ok = applyPatches(s, patches, 2, nullptr);
        TEST("applyPatches: mixed critical missing returns false", !ok);
    }
}

void test_critical_anchors_detected() {
    // Temporarily rename shader files to force buildFragmentShader to find
    // a blackhole.glsl without the critical anchors, then restore.
    // Since we can't write to the source tree during tests, we test the
    // buildFragmentShader logic by directly creating a fixture: a minimal
    // shader body that's missing the critical anchor.

    char cwd[4096];
    getcwd(cwd, sizeof(cwd));
    chdir("/tmp");

    // Create a minimal blackhole.glsl WITHOUT "float t = iTime * DRIFT_SPEED;"
    std::string badBody = createTempFile(
        "float t = iTime * SOMETHING_ELSE;\nvoid mainImage() {}\n");
    std::string goodBody = createTempFile(
        "float t = iTime * DRIFT_SPEED;\nvoid mainImage() {}\n");
    std::string header = createTempFile(
        "uniform float iTime;\n");
    std::string badHeader = createTempFile(
        "uniform float iTime;\n"); // good header, good enough

    if (!badBody.empty() && !goodBody.empty() && !header.empty()) {
        // We can't test buildFragmentShader directly because it reads from
        // fixed paths "blackhole.glsl" and "shaders/frag_desktop_header.glsl".
        // Instead, the applyPatches test above already validates the mechanism.
        //
        // For buildFragmentShader, we verify by checking that the critical
        // anchors ARE present in the actual shader files by reading them:
        chdir(cwd);
        std::string realBody = readFile("blackhole.glsl");
        if (!realBody.empty()) {
            bool hasTimeAnchor = realBody.find("float t = iTime * DRIFT_SPEED;") != std::string::npos;
            TEST("critical_anchor: blackhole.glsl has 'float t = iTime * DRIFT_SPEED;'", hasTimeAnchor);
        }
        std::string realHeader = readFile("shaders/frag_desktop_header.glsl");
        if (!realHeader.empty()) {
            bool hasUniformIime = realHeader.find("uniform float iTime;") != std::string::npos;
            TEST("critical_anchor: header has 'uniform float iTime;'", hasUniformIime);
        }
    }

    deleteTempFile(badBody);
    deleteTempFile(goodBody);
    deleteTempFile(header);
    deleteTempFile(badHeader);
}

int main() {
    fprintf(stdout, "=== shader_source tests ===\n");

    test_readFile();
    test_buildFragmentShader_missing_files();
    test_preset_count_limits();
    test_version_330();
    test_applyPatches();
    test_critical_anchors_detected();

    fprintf(stdout, "\nResults: %d passed, %d failed\n", testsPassed, testsFailed);
    return testsFailed > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
