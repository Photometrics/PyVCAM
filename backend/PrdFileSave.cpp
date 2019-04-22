#include "PrdFileSave.h"

/* System */
#include <cstring>

pm::PrdFileSave::PrdFileSave(const std::string& fileName, const PrdHeader& header)
    : FileSave(fileName, header),
    m_file()
{
}

pm::PrdFileSave::~PrdFileSave()
{
    if (IsOpen())
        Close();
}

bool pm::PrdFileSave::Open()
{
    if (IsOpen())
        return true;

    m_file.open(m_fileName, std::ios_base::out | std::ios_base::binary);
    if (!m_file.is_open())
        return false;

    m_frameIndex = 0;

    return IsOpen();
}

bool pm::PrdFileSave::IsOpen() const
{
    return m_file.is_open();
}

void pm::PrdFileSave::Close()
{
    if (m_header.frameCount != m_frameIndex)
    {
        m_header.frameCount = m_frameIndex;

        m_file.seekp(0);
        m_file.write((char*)&m_header, sizeof(PrdHeader));
        m_file.seekp(0, std::ios_base::end);
    }

    m_file.flush();
    m_file.close();

    FileSave::Close();
}

bool pm::PrdFileSave::WriteFrame(const void* metaData,
        const void* extDynMetaData, const void* rawData)
{
    if (!FileSave::WriteFrame(metaData, extDynMetaData, rawData))
        return false;

    // Write PRD header to file only once at the beginning
    if (m_file.tellp() == std::ofstream::pos_type(0))
    {
        m_file.write((char*)&m_header, sizeof(PrdHeader));
        if (!m_file.good())
            return false;
    }

    m_file.write(static_cast<const char*>(metaData),
            m_header.sizeOfPrdMetaDataStruct);
    if (!m_file.good())
        return false;

    if (m_header.version >= PRD_VERSION_0_5)
    {
        auto prdMetaData = static_cast<const PrdMetaData*>(metaData);

        if (prdMetaData->extDynMetaDataSize > 0 && extDynMetaData)
        {
            m_file.write(static_cast<const char*>(extDynMetaData),
                    prdMetaData->extDynMetaDataSize);
            if (!m_file.good())
                return false;
        }
    }

    m_file.write(static_cast<const char*>(rawData), m_rawDataBytes);
    if (!m_file.good())
        return false;

    m_frameIndex++;
    return true;
}

bool pm::PrdFileSave::WriteFrame(const Frame& frame, uint32_t expTime)
{
    if (!FileSave::WriteFrame(frame, expTime))
        return false;

    return WriteFrame(m_framePrdMetaData, m_framePrdExtDynMetaData, frame.GetData());
}
