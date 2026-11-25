"""Cross-build the verified Bonsai CLI with Win32 driver and file mapping."""
from pathlib import Path
import sys

source, output = map(Path, sys.argv[1:])
text = source.read_text()

def replace(old, new):
    global text
    assert text.count(old) == 1, old
    text = text.replace(old, new)

replace("#include <sys/mman.h>", "#include <windows.h>")
replace("#include <dlfcn.h>", "")
replace('    void *library = dlopen("libcuda.so.1", RTLD_NOW | RTLD_LOCAL);',
        '    HMODULE library = LoadLibraryExW(L"nvcuda.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);')
replace('fprintf(stderr, "Bonsai CUDA driver: %s\\n", dlerror());',
        'fprintf(stderr, "Bonsai CUDA driver: Windows error %lu\\n", (unsigned long)GetLastError());')
replace("dlsym(library, BONSAI_STRING(name))", "GetProcAddress(library, BONSAI_STRING(name))")
replace("dlclose(library);", "FreeLibrary(library);")
first = text.index("    int fd = open(gguf, O_RDONLY);")
last = text.index("    if (loaded != 0)", first)
text = text[:first] + '''    HANDLE file = CreateFileA(gguf, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Cannot open GGUF: Windows error %lu\\n", (unsigned long)GetLastError());
        goto error;
    }
    LARGE_INTEGER info;
    if (!GetFileSizeEx(file, &info) || info.QuadPart < 24) {
        CloseHandle(file);
        goto error;
    }
    HANDLE view = CreateFileMappingW(file, NULL, PAGE_READONLY, 0, 0, NULL);
    CloseHandle(file);
    if (!view) goto error;
    uint8_t *mapping = MapViewOfFile(view, FILE_MAP_READ, 0, 0, 0);
    CloseHandle(view);
    if (!mapping) goto error;
    const int loaded = load_weights(m, mapping, (size_t)info.QuadPart);
    UnmapViewOfFile(mapping);
''' + text[last:]
output.write_text(text)
