

TARGET = pyvcam_test.exe

SOURCES = *.cpp \
    ../*.cpp

all: $(TARGET)

.PHONY: clean
clean:
    rm -f *.obj pyvcam_test.*

$(TARGET):$(SOURCES)
    cl \
    /I %USERPROFILE%\src\googletest\googletest\include \
    /I "%PVCAM_SDK_PATH%\Inc" \
    /I ".." \
    /EHsc /Zi /MTd /D_ITERATOR_DEBUG_LEVEL=2 /Fe:$@ \
    gtestd.lib \
    $** \
    "%PVCAM_SDK_PATH%lib\amd64\pvcam64.lib"
