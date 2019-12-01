// Copyright (c) 2019 Nicholas Corgan
// SPDX-License-Identifier: BSD-3-Clause

#include "DeviceCache.hpp"
#include "Utility.hpp"

#include <Pothos/Exception.hpp>
#include <Pothos/Object.hpp>

#include <Poco/Format.h>

#include <algorithm>

void setThreadAFBackend(af::Backend backend)
{
    // This check is constexpr, so in theory, the compiler should be able
    // able to optimize away the invalid case.
    if(isAFConfigPerThread())
    {
        af::setBackend(backend);
        assert(backend == af::getActiveBackend());
    }
    else
    {
        throw Pothos::RuntimeException(
                  Poco::format(
                      "This build of PothosArrayFire was built against ArrayFire %s, "
                      "which only supports a single global backend. To enable setting "
                      "different backends per block, rebuild this module against "
                      "ArrayFire 3.5+.",
                      std::string(AF_VERSION)));
    }
}

void setThreadAFDevice(const std::string& device)
{
    // This check is constexpr, so in theory, the compiler should be able
    // able to optimize away the invalid case.
    if(isAFConfigPerThread())
    {
        const auto backend = af::getActiveBackend();
        const auto& deviceCache = getDeviceCache();

        // Search for the given device name in our current backend.
        auto deviceIter = std::find_if(
                              deviceCache.begin(),
                              deviceCache.end(),
                              [&backend, &device](const DeviceCacheEntry& entry)
                              {
                                  return (entry.afBackendEnum == backend) &&
                                         (entry.name == device);
                              });
        if(deviceIter != deviceCache.end())
        {
            af::setDevice(deviceIter->afDeviceIndex);
            assert(deviceIter->afDeviceIndex == af::getDevice());
        }
        else
        {
            throw Pothos::NotFoundException(
                      Poco::format(
                          "Could not find device with backend %s and name %s.",
                          Pothos::Object(backend).convert<std::string>(),
                          device));
        }
    }
    else
    {
        throw Pothos::RuntimeException(
                  Poco::format(
                      "This build of PothosArrayFire was built against ArrayFire %s, "
                      "which only supports a single global device. To enable setting "
                      "different devices per block, rebuild this module against "
                      "ArrayFire 3.5+.",
                      std::string(AF_VERSION)));
    }
}

void validateDType(
    const Pothos::DType& dtype,
    const DTypeSupport& supportedTypes)
{
    // Make sure *something* is supported.
    assert(supportedTypes.supportInt ||
           supportedTypes.supportUInt ||
           supportedTypes.supportFloat ||
           supportedTypes.supportComplexFloat);

    // Specific error for types not supported by any block
    static const std::vector<std::string> globalUnsupportedTypes =
    {
        "int8",
        "complex_int8",
        "complex_int16",
        "complex_int32",
        "complex_int64",
        "complex_uint8",
        "complex_uint16",
        "complex_uint32",
        "complex_uint64",
    };
    if(doesVectorContainValue(globalUnsupportedTypes, dtype.name()))
    {
        throw Pothos::InvalidArgumentException(
                  "PothosArrayFire blocks do not support this type",
                  dtype.name());
    }

    const bool isDTypeSupported = (isDTypeInt(dtype) && supportedTypes.supportInt) ||
                                  (isDTypeUInt(dtype) && supportedTypes.supportUInt) ||
                                  (isDTypeFloat(dtype) && supportedTypes.supportFloat) ||
                                  (isDTypeComplexFloat(dtype) && supportedTypes.supportComplexFloat);

    if(!isDTypeSupported)
    {
        throw Pothos::InvalidArgumentException(
                  "Unsupported type",
                  dtype.name());
    }
}

void validateComplexAndFloatTypesMatch(
    const Pothos::DType& complexDType,
    const Pothos::DType& floatDType)
{
    assert(!complexDType.isInteger());
    assert(complexDType.isComplex());
    assert(!floatDType.isInteger());
    assert(!floatDType.isComplex());

    static constexpr size_t subStringStart = 8; // Cut off "complex_"

    auto complexDTypeSubtype = complexDType.name().substr(subStringStart);
    auto floatType = floatDType.name();

    if(complexDTypeSubtype != floatType)
    {
        throw Pothos::InvalidArgumentException(
                  "Incompatible types",
                  (complexDTypeSubtype + ", " + floatType));
    }
}

Pothos::Object getArrayIndexOfUnknownType(
    const af::array& afArray,
    dim_t index)
{
    const auto& arrIndex = afArray(index);
    Pothos::Object ret;

    assert(arrIndex.elements() == 1);

    #define SwitchCase(afDType, ctype) \
        case afDType: \
            ret = Pothos::Object(arrIndex.scalar<PothosToAF<ctype>::type>()).convert(typeid(ctype)); \
            break;

    switch(afArray.type())
    {
        SwitchCase(::s16, std::int16_t)
        SwitchCase(::s32, std::int32_t)
        SwitchCase(::s64, long long)
        SwitchCase(::u8,  std::uint8_t)
        SwitchCase(::u16, std::uint16_t)
        SwitchCase(::u32, std::uint32_t)
        SwitchCase(::u64, unsigned long long)
        SwitchCase(::f32, float)
        SwitchCase(::f64, double)
        SwitchCase(::c32, std::complex<float>)
        SwitchCase(::c64, std::complex<double>)

        default:
            throw Pothos::AssertionViolationException("Invalid dtype");
            break;
    }

    return ret;
}
