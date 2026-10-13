#include "FrameText.h"

#include <cstdint>
#include <string>

namespace stm32_audio::host {

namespace {

const char* commandName(std::uint8_t command)
{
    switch (command) {
    case PROTO_CMD_GET_DEVICE_LIST:
        return "GET_DEVICE_LIST";
    case PROTO_CMD_DEVICE_LIST:
        return "DEVICE_LIST";
    case PROTO_CMD_SELECT_DEVICE:
        return "SELECT_DEVICE";
    case PROTO_CMD_DEVICE_CHANGED:
        return "DEVICE_CHANGED";
    case PROTO_CMD_SET_VOLUME:
        return "SET_VOLUME";
    case PROTO_CMD_VOLUME_CHANGED:
        return "VOLUME_CHANGED";
    case PROTO_CMD_SET_MUTE:
        return "SET_MUTE";
    case PROTO_CMD_MUTE_CHANGED:
        return "MUTE_CHANGED";
    case PROTO_CMD_ERROR:
        return "ERROR";
    default:
        return "UNKNOWN";
    }
}

const char* iconName(std::uint8_t icon)
{
    static const char* const names[] = {"generic", "speakers", "headphones", "headset", "display", "digital"};
    return icon < PROTO_ICON_COUNT ? names[icon] : "?";
}

const char* errorName(std::uint8_t code)
{
    switch (code) {
    case PROTO_ERROR_BAD_FRAME:
        return "BAD_FRAME";
    case PROTO_ERROR_UNSUPPORTED_VERSION:
        return "UNSUPPORTED_VERSION";
    case PROTO_ERROR_UNKNOWN_COMMAND:
        return "UNKNOWN_COMMAND";
    case PROTO_ERROR_BAD_PAYLOAD:
        return "BAD_PAYLOAD";
    case PROTO_ERROR_UNKNOWN_DEVICE:
        return "UNKNOWN_DEVICE";
    case PROTO_ERROR_OPERATION_FAILED:
        return "OPERATION_FAILED";
    case PROTO_ERROR_BUSY:
        return "BUSY";
    default:
        return "?";
    }
}

std::string indexText(std::uint8_t index)
{
    return index == PROTO_NO_DEVICE ? std::string("none") : std::to_string(index);
}

}  // namespace

std::string describeMessage(const ProtoMessage& msg)
{
    std::string out = commandName(msg.command);
    out += " seq=" + std::to_string(msg.sequence);

    switch (msg.command) {
    case PROTO_CMD_DEVICE_LIST: {
        const ProtoDeviceEntry& d = msg.u.device;
        out += " gen=" + std::to_string(d.generation);
        if (d.total == 0U) {
            out += " empty";
            break;
        }
        out += " " + std::to_string(d.position + 1U) + "/" + std::to_string(d.total);
        out += " index=" + std::to_string(d.device_index);
        if ((d.flags & PROTO_DEVICE_FLAG_DEFAULT) != 0U) {
            out += " default";
        }
        out += " icon=";
        out += iconName(d.icon);
        out += " \"";
        out.append(d.name, d.name_len);
        out += "\"";
        break;
    }
    case PROTO_CMD_SELECT_DEVICE:
    case PROTO_CMD_DEVICE_CHANGED:
        out += " index=" + indexText(msg.u.device_index);
        break;
    case PROTO_CMD_SET_VOLUME:
    case PROTO_CMD_VOLUME_CHANGED:
        out += " volume=" + std::to_string(msg.u.volume);
        break;
    case PROTO_CMD_SET_MUTE:
    case PROTO_CMD_MUTE_CHANGED:
        out += msg.u.muted != 0U ? " muted=1" : " muted=0";
        break;
    case PROTO_CMD_ERROR:
        out += " code=";
        out += errorName(msg.u.error.code);
        out += " ref_cmd=" + std::to_string(msg.u.error.ref_command);
        out += " ref_seq=" + std::to_string(msg.u.error.ref_sequence);
        break;
    default:
        break;
    }
    return out;
}

}  // namespace stm32_audio::host
