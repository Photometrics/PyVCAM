#include "FileLoad.h"

/* System */
#include <cstring>

pm::FileLoad::FileLoad(const std::string& fileName, PrdHeader& header)
    : File(fileName),
    m_header(header),
    m_rawDataBytes(0),
    m_metaData(nullptr),
    m_extDynMetaData(nullptr),
    m_rawData(nullptr)
{
    memset(&m_header, 0, sizeof(PrdHeader));
}

pm::FileLoad::~FileLoad()
{
}

PrdHeader& pm::FileLoad::GetHeader() const
{
    return m_header;
}

void pm::FileLoad::Close()
{
    std::free(m_metaData);
    std::free(m_extDynMetaData);
    std::free(m_rawData);
}

bool pm::FileLoad::ReadFrame(const void** metaData, const void** extDynMetaData,
            const void** rawData)
{
    if (!IsOpen())
        return false;

    if (!metaData || !extDynMetaData || !rawData)
        return false;

    if (m_rawDataBytes == 0 || m_frameIndex >= m_header.frameCount)
        return false;

    *metaData = nullptr;
    *extDynMetaData = nullptr;
    *rawData = nullptr;

    return true;
}
