// Ninjutso Sora V2 1K receiver. Protocol from NinjaForce's bundled
// out/renderer/assets/createSoraV2-DL6g5ddD.js, createSorav2.readState().
// Feature report 05, command 15: [9] battery (0 means 100%), [10] charging,
// [12] online. Indices include the report ID, as in WebHID's returned DataView.
// Ten Air receiver: NinjaForce's PixArt driver at https://www.ninjaforce.co/_nuxt/CaGpxWb-.js
// uses feature report 06. Each status command returns one value in byte 8.
#include "drivers.h"

namespace {
void PollSoraV2(PollContext& ctx) {
    constexpr USHORT vid = 0x1915, pid = 0xAE1C;
    if (ctx.IsClaimed(vid, pid)) return;
    for (const HidColl* c : ctx.Find(vid, pid, [](const HidColl& c) {
             return c.page() == 0xFFA0 && c.usage() == 1 && c.featLen() >= 32;
         })) {
        HidDev d;
        if (!d.OpenFeatureOnly(*c)) continue;
        std::vector<BYTE> req(32, 0);
        req[0] = 0x05;
        req[1] = 0x15;
        req[4] = 1;
        DeviceStatus s;
        bool answered = false;
        for (int attempt = 0; attempt < 4; ++attempt) {
            if (attempt) Sleep(90);
            if (!d.SetFeature(req)) break;
            Sleep(15);
            std::vector<BYTE> r;
            if (!d.GetFeature(0x05, r) || r.size() < 13 || r[0] != 0x05 || r[1] != 0x15 || r[9] > 100) continue;
            answered = true;
            s.connected = r[12] != 0;
            s.percent = s.connected ? (r[9] == 0 ? 100 : r[9]) : -1;
            s.charging = s.connected && r[10] != 0;
            if (s.connected) break;
        }
        if (!answered) continue;
        s.key = L"ninjutso:" + Hex4(vid) + L":" + Hex4(pid);
        s.name = L"Ninjutso Sora V2";
        s.kind = DeviceKind::Mouse;
        s.via = L"2.4 GHz receiver";
        ctx.Add(s, vid, pid);
        break;
    }
}

int ReadTenValue(HidDev& device, BYTE command) {
    constexpr BYTE report = 0x06;
    for (int attempt = 0; attempt < 4; ++attempt) {
        if (!device.SetFeature({report, command, 0, 0, 1})) break;
        Sleep(attempt == 0 ? 30 : 60);  // NinjaForce retries when the previous command is still in the buffer.
        std::vector<BYTE> reply;
        if (device.GetFeature(report, reply) && reply.size() > 8 && reply[0] == report &&
            reply[1] == command && reply[6] == 1) return reply[8];
    }
    return -1;
}

void PollTenReceiver(PollContext& ctx) {
    constexpr USHORT vid = 0x093A, pid = 0xEB01;
    if (ctx.IsClaimed(vid, pid)) return;
    for (const HidColl* c : ctx.Find(vid, pid, [](const HidColl& c) {
             // The receiver also exposes a 64-byte feature collection with the same page and usage.
             return c.page() == 0xFF01 && c.usage() == 1 && c.featLen() == 16;
         })) {
        HidDev d;
        if (!d.OpenFeatureOnly(*c)) continue;
        const int online = ReadTenValue(d, 0x13);
        if (online != 0 && online != 1) continue;
        DeviceStatus s;
        s.connected = online == 1;
        if (s.connected) {
            s.percent = ReadTenValue(d, 0x12);
            if (s.percent < 0 || s.percent > 100) continue;
            s.charging = ReadTenValue(d, 0x11) > 0;
            s.graceSec = 180;  // Keep the last reading dimmed across a missed receiver poll.
        }
        s.key = L"ninjutso:" + Hex4(vid) + L":" + Hex4(pid);
        s.name = HidProduct(*c);
        if (s.name.empty()) s.name = L"Ninjutso Ten series";
        s.kind = DeviceKind::Mouse;
        s.via = L"2.4 GHz receiver";
        ctx.Add(s, vid, pid);
        break;
    }
}
}  // namespace

void PollNinjutso(PollContext& ctx) {
    PollSoraV2(ctx);
    PollTenReceiver(ctx);
}
