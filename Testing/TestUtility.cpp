// Copyright (c) 2020 Nicholas Corgan
// SPDX-License-Identifier: BSD-3-Clause

#include "DeviceCache.hpp"
#include "TestUtility.hpp"

#include <Poco/Random.h>

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

template <typename T>
static inline EnableIfNotComplex<T, T> epsilon()
{
    return T(1e-6);
}

template <typename T>
static inline EnableIfComplex<T, T> epsilon()
{
    using ScalarType = typename T::value_type;

    return T(epsilon<ScalarType>(), epsilon<ScalarType>());
}

// TODO: consolidate all these testBufferChunk calls
template <typename T>
static EnableIfAnyInt<T, void> compareBufferChunks(
    const Pothos::BufferChunk& expectedBufferChunk,
    const Pothos::BufferChunk& actualBufferChunk)
{
    POTHOS_TEST_EQUALA(
        expectedBufferChunk.as<const T*>(),
        actualBufferChunk.as<const T*>(),
        expectedBufferChunk.elements());
}

template <typename T>
static EnableIfFloat<T, void> compareBufferChunks(
    const Pothos::BufferChunk& expectedBufferChunk,
    const Pothos::BufferChunk& actualBufferChunk)
{
    POTHOS_TEST_CLOSEA(
        expectedBufferChunk.as<const T*>(),
        actualBufferChunk.as<const T*>(),
        expectedBufferChunk.elements(),
        T(1e-6));
}

template <typename T>
static EnableIfComplex<T, void> compareBufferChunks(
    const Pothos::BufferChunk& expectedBufferChunk,
    const Pothos::BufferChunk& actualBufferChunk)
{
    using ScalarType = typename T::value_type;

    auto scalarExpected = expectedBufferChunk;
    scalarExpected.dtype = typeid(ScalarType);

    auto scalarActual = actualBufferChunk;
    scalarActual.dtype = typeid(ScalarType);

    compareBufferChunks<ScalarType>(
        scalarExpected,
        scalarActual);
}

void testBufferChunk(
    const Pothos::BufferChunk& expectedBufferChunk,
    const Pothos::BufferChunk& actualBufferChunk)
{
    POTHOS_TEST_EQUAL(
        expectedBufferChunk.dtype,
        actualBufferChunk.dtype);
    POTHOS_TEST_EQUAL(
        expectedBufferChunk.elements(),
        actualBufferChunk.elements());

    #define IfTypeThenCompare(typeStr, cType) \
        if(expectedBufferChunk.dtype.name() == typeStr) \
        { \
            compareBufferChunks<cType>( \
                expectedBufferChunk, \
                actualBufferChunk); \
            return; \
        }

    IfTypeThenCompare("int16", short)
    IfTypeThenCompare("int32", int)
    IfTypeThenCompare("int64", long long)
    IfTypeThenCompare("uint8", unsigned char)
    IfTypeThenCompare("uint16", unsigned short)
    IfTypeThenCompare("uint32", unsigned)
    IfTypeThenCompare("uint64", unsigned long long)
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

    IfTypeThenAdd("int8", char)
    IfTypeThenAdd("int16", short)
    IfTypeThenAdd("int32", int)
    IfTypeThenAdd("int64", long long)
    IfTypeThenAdd("uint8", unsigned char)
    IfTypeThenAdd("uint16", unsigned short)
    IfTypeThenAdd("uint32", unsigned)
    IfTypeThenAdd("uint64", unsigned long long)
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
    const auto afDType = Pothos::Object(Pothos::DType(type)).convert<af::dtype>();

    af::setSeed(Poco::Random().next());

    return Pothos::Object(af::randu(TestInputLength, afDType)).convert<Pothos::BufferChunk>();
}

#define RETURN_OBJECT(typeStr, cType) \
    if(type == typeStr) \
        return Pothos::Object(getSingleTestInput<cType>());

Pothos::Object getSingleTestInput(const std::string& type)
{
    // ArrayFire doesn't support int8
    RETURN_OBJECT("int16", short)
    RETURN_OBJECT("int32", int)
    RETURN_OBJECT("int64", long long)
    RETURN_OBJECT("uint8", unsigned char)
    RETURN_OBJECT("uint16", unsigned short)
    RETURN_OBJECT("uint32", unsigned)
    RETURN_OBJECT("uint64", unsigned long long)
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

std::vector<Pothos::BufferChunk> convert2DAfArrayToBufferChunks(const af::array& afArray)
{
    POTHOS_TEST_EQUAL(2, afArray.numdims());
    const auto numRows = afArray.dims(0);

    std::vector<Pothos::BufferChunk> bufferChunks;
    for(dim_t i = 0; i < numRows; ++i)
    {
        bufferChunks.emplace_back(Pothos::Object(afArray.row(i)).convert<Pothos::BufferChunk>());
    }

    return bufferChunks;
}

// Assumption: all BufferChunks are of same type and size
af::array convertBufferChunksTo2DAfArray(const std::vector<Pothos::BufferChunk>& bufferChunks)
{
    POTHOS_TEST_FALSE(bufferChunks.empty());

    const auto afDType = Pothos::Object(bufferChunks[0].dtype).convert<af::dtype>();
    af::dim4 dims(dim_t(bufferChunks.size()), dim_t(bufferChunks[0].elements()));

    af::array afArray(dims, afDType);
    for(dim_t row = 0; row < dim_t(bufferChunks.size()); ++row)
    {
        afArray.row(row) = Pothos::Object(bufferChunks[row]).convert<af::array>();
    }

    return afArray;
}

}
