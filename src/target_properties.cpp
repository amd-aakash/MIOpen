/*******************************************************************************
 *
 * MIT License
 *
 * Copyright (c) 2020 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *******************************************************************************/
#include <miopen/env.hpp>
#include <miopen/handle.hpp>
#include <miopen/stringutils.hpp>
#include <miopen/target_properties.hpp>
#include <map>
#include <string>

#define WORKAROUND_ISSUE_1204 1 // ROCm may incorrectly report "sramecc-" for gfx900.

MIOPEN_DECLARE_ENV_VAR(MIOPEN_DEBUG_ENFORCE_DEVICE)
MIOPEN_DECLARE_ENV_VAR(MIOPEN_DEVICE_ARCH)

namespace miopen {

static std::string GetDeviceNameFromMap(const std::string& in)
{
    static const std::map<std::string, std::string> device_name_map = {
        {"Ellesmere", "gfx803"},
        {"Baffin", "gfx803"},
        {"RacerX", "gfx803"},
        {"Polaris10", "gfx803"},
        {"Polaris11", "gfx803"},
        {"Tonga", "gfx803"},
        {"Fiji", "gfx803"},
        {"gfx800", "gfx803"},
        {"gfx802", "gfx803"},
        {"gfx804", "gfx803"},
        {"Vega10", "gfx900"},
        {"gfx901", "gfx900"},
        {"10.3.0 Sienna_Cichlid 18", "gfx1030"},
    };

    const char* const p_asciz = miopen::GetStringEnv(MIOPEN_DEBUG_ENFORCE_DEVICE{});
    if(p_asciz != nullptr && strlen(p_asciz) > 0)
        return {p_asciz};

    const auto name = in.substr(0, in.find(':')); // str.substr(0, npos) returns str.

    auto match = device_name_map.find(name);
    if(match != device_name_map.end())
        return match->second;
    return name; // NOLINT (performance-no-automatic-move)
}

// See https://github.com/llvm/llvm-project/commit/1ed4caff1d5cd49233c1ae7b9f6483a946ed5eea.
const std::size_t TargetProperties::MaxWaveScratchSize =
    (static_cast<const std::size_t>(256) * 4) * ((1 << 13) - 1);

void TargetProperties::Init(const Handle* const handle)
{
    const auto rawName = [&]() -> std::string {
        const char* const arch = miopen::GetStringEnv(MIOPEN_DEVICE_ARCH{});
        if(arch != nullptr && strlen(arch) > 0)
            return arch;
        return handle->GetDeviceNameImpl();
    }();
    name = GetDeviceNameFromMap(rawName);
    // DKMS driver older than 5.9 may report incorrect state of SRAMECC feature.
    // Therefore we compute default SRAMECC and rely on it for now.
    sramecc = [&]() -> boost::optional<bool> {
        if(name == "gfx906" || name == "gfx908")
            return {true};
        return {};
    }();
    // However we need to store the reported state, even if it is incorrect,
    // to use together with COMGR.
    sramecc_reported = [&]() -> boost::optional<bool> {
#if WORKAROUND_ISSUE_1204
        if(name == "gfx900")
            return {};
#endif
        if(rawName.find(":sramecc+") != std::string::npos)
            return true;
        if(rawName.find(":sramecc-") != std::string::npos)
            return false;
        return sramecc; // default
    }();
    xnack = [&]() -> boost::optional<bool> {
        if(rawName.find(":xnack+") != std::string::npos)
            return true;
        if(rawName.find(":xnack-") != std::string::npos)
            return false;
        return {}; // default
    }();
    InitDbId();
}

void TargetProperties::InitDbId()
{
    dbId = name;
    if(name == "gfx906" || name == "gfx908")
    {
        // Let's stay compatible with existing gfx906/908 databases.
        // When feature equal to the default (SRAMECC ON), do not
        // append feature suffix. This is for backward compatibility
        // with legacy databases ONLY!
        if(!sramecc || !(*sramecc))
            dbId += "_nosramecc";
    }
    else
    {
        if(sramecc && *sramecc)
            dbId += "_sramecc";
    }
    if(xnack && *xnack)
        dbId += "_xnack";
}

} // namespace miopen
