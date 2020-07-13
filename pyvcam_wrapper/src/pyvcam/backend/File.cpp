#include "File.h"

pm::File::File(const std::string& fileName)
    : m_fileName(fileName),
    m_frameIndex(0)
{
}

pm::File::~File()
{
}

const std::string& pm::File::GetFileName() const
{
    return m_fileName;
}
