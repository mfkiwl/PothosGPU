// Copyright (c) 2020 Nicholas Corgan
// SPDX-License-Identifier: BSD-3-Clause

#include "DeviceCache.hpp"
#include "TestUtility.hpp"

#include <Pothos/Framework.hpp>
#include <Pothos/Plugin.hpp>
#include <Pothos/Proxy.hpp>
#include <Pothos/Testing.hpp>

namespace GPUTests
{

void setupTestEnv()
{
    af::setBackend(getAvailableBackends()[0]);
}

void testBufferChunk(
    const Pothos::BufferChunk& expectedBufferChunk,
    const Pothos::BufferChunk& actualBufferChunk)
{
    POTHOS_TEST_EQUAL(
        expectedBufferChunk.dtype.name(),
        actualBufferChunk.dtype.name());
    POTHOS_TEST_EQUAL(
        expectedBufferChunk.elements(),
        actualBufferChunk.elements());

    #define IfTypeThenCompare(typeStr, cType) \
        if(expectedBufferChunk.dtype.name() == typeStr) \
        { \
            POTHOS_TEST_EQUALA( \
                expectedBufferChunk.as<const cType*>(), \
                actualBufferChunk.as<const cType*>(), \
                expectedBufferChunk.elements()); \
            return; \
        }

    IfTypeThenCompare("int8", std::int8_t)
    IfTypeThenCompare("int16", std::int16_t)
    IfTypeThenCompare("int32", std::int32_t)
    IfTypeThenCompare("int64", std::int64_t)
    IfTypeThenCompare("uint8", std::uint8_t)
    IfTypeThenCompare("uint16", std::uint16_t)
    IfTypeThenCompare("uint32", std::uint32_t)
    IfTypeThenCompare("uint64", std::uint64_t)
    IfTypeThenCompare("float32", float)
    IfTypeThenCompare("float64", double)
    IfTypeThenCompare("complex_float32", std::complex<float>)
    IfTypeThenCompare("complex_float64", std::complex<double>)
}

void addMinMaxToAfArray(af::array& rAfArray, const std::string& type)
{
    #define IfTypeThenAdd(typeStr, ctype) \
        if(typeStr == type) \
        { \
            addMinMaxToAfArray<ctype>(rAfArray); \
            return; \
        }

    IfTypeThenAdd("int8", std::int8_t)
    IfTypeThenAdd("int16", std::int16_t)
    IfTypeThenAdd("int32", std::int32_t)
    IfTypeThenAdd("int64", std::int64_t)
    IfTypeThenAdd("uint8", std::uint8_t)
    IfTypeThenAdd("uint16", std::uint16_t)
    IfTypeThenAdd("uint32", std::uint32_t)
    IfTypeThenAdd("uint64", std::uint64_t)
    IfTypeThenAdd("float32", float)
    IfTypeThenAdd("float64", double)
    IfTypeThenAdd("complex_float32", std::complex<float>)
    IfTypeThenAdd("complex_float64", std::complex<double>)
}

#define RETURN_BUFFERCHUNK(typeStr, cType) \
    if(type == typeStr) \
        return stdVectorToBufferChunk(getTestInputs<cType>());

Pothos::BufferChunk getTestInputs(const std::string& type)
{
    // ArrayFire doesn't support int8
    RETURN_BUFFERCHUNK("int16", std::int16_t)
    RETURN_BUFFERCHUNK("int32", std::int32_t)
    RETURN_BUFFERCHUNK("int64", std::int64_t)
    RETURN_BUFFERCHUNK("uint8", std::uint8_t)
    RETURN_BUFFERCHUNK("uint16", std::uint16_t)
    RETURN_BUFFERCHUNK("uint32", std::uint32_t)
    RETURN_BUFFERCHUNK("uint64", std::uint64_t)
    RETURN_BUFFERCHUNK("float32", float)
    RETURN_BUFFERCHUNK("float64", double)
    // ArrayFire doesn't support any integral complex type
    RETURN_BUFFERCHUNK("complex_float32", std::complex<float>)
    RETURN_BUFFERCHUNK("complex_float64", std::complex<double>)

    // Should never happen
    return Pothos::BufferChunk();
}

#define RETURN_OBJECT(typeStr, cType) \
    if(type == typeStr) \
        return Pothos::Object(getSingleTestInput<cType>());

Pothos::Object getSingleTestInput(const std::string& type)
{
    // ArrayFire doesn't support int8
    RETURN_OBJECT("int16", std::int16_t)
    RETURN_OBJECT("int32", std::int32_t)
    RETURN_OBJECT("int64", std::int64_t)
    RETURN_OBJECT("uint8", std::uint8_t)
    RETURN_OBJECT("uint16", std::uint16_t)
    RETURN_OBJECT("uint32", std::uint32_t)
    RETURN_OBJECT("uint64", std::uint64_t)
    RETURN_OBJECT("float32", float)
    RETURN_OBJECT("float64", double)
    // ArrayFire doesn't support any integral complex type
    RETURN_OBJECT("complex_float32", std::complex<float>)
    RETURN_OBJECT("complex_float64", std::complex<double>)

    // Should never happen
    return Pothos::Object();
}

const std::vector<std::string>& getAllDTypeNames()
{
    static const std::vector<std::string> AllTypes =
    {
        // ArrayFire doesn't support int8
        "int16",
        "int32",
        "int64",
        "uint8",
        "uint16",
        "uint32",
        "uint64",
        "float32",
        "float64",
        // ArrayFire doesn't support complex integral types
        "complex_float32",
        "complex_float64"
    };
    return AllTypes;
}

}
