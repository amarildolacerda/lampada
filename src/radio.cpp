#ifdef PORTAL
#include "radio.h"

RadioFrame::RadioFrame()
{
    clear();
}

void RadioFrame::clear()
{
    origin = 0;
    destination = 0;
    length = 0;
    hope = 0;
    memset(payload, 0, sizeof(payload));
}

bool RadioFrame::setPayload(const uint8_t *data, size_t size)
{
    if (data == nullptr || size == 0 || size > RADIO_PAYLOAD_MAX)
    {
        return false;
    }

    length = size;
    memcpy(payload, data, length);
    return true;
}

bool RadioFrame::setPayload(const String &data)
{
    size_t size = data.length();
    if (size == 0 || size > RADIO_PAYLOAD_MAX)
    {
        return false;
    }

    length = size;
    memcpy(payload, data.c_str(), length);
    return true;
}

String RadioFrame::payloadAsString() const
{
    return String((const char *)payload).substring(0, length);
}

size_t RadioFrame::serialize(uint8_t *buffer, size_t bufferSize) const
{
    if (buffer == nullptr || bufferSize < requiredSize(length) || length > RADIO_PAYLOAD_MAX)
    {
        return 0;
    }

    buffer[0] = origin;
    buffer[1] = destination;
    buffer[2] = hope;
    buffer[3] = length;
    memcpy(buffer + 4, payload, length);
    return requiredSize(length);
}

bool RadioFrame::deserialize(const uint8_t *buffer, size_t bufferSize)
{
    if (buffer == nullptr || bufferSize < 4)
    {
        return false;
    }

    uint8_t newLength = buffer[3];
    if (newLength > RADIO_PAYLOAD_MAX || bufferSize < requiredSize(newLength))
    {
        return false;
    }

    origin = buffer[0];
    destination = buffer[1];
    hope = buffer[2];
    length = newLength;
    memcpy(payload, buffer + 4, length);
    return true;
}
#endif