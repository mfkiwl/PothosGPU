// Copyright (c) 2019 Nicholas Corgan
// SPDX-License-Identifier: BSD-3-Clause

#include "ArrayFireBlock.hpp"
#include "Utility.hpp"

#include <Pothos/Framework.hpp>
#include <Pothos/Object.hpp>

#include <Poco/Logger.h>

#include <arrayfire.h>

#include <cmath>
#include <functional>
#include <string>
#include <typeinfo>

//
// Misc
//

static bool isPowerOfTwo(size_t num)
{
    if(0 == num)
    {
        return false;
    }

    return (std::ceil(std::log2(num)) == std::floor(std::log2(num)));
}

static const std::string fftBlockPath = "/arrayfire/signal/fft";
static const std::string rfftBlockPath = "/arrayfire/signal/rfft";

//
// Block classes
//

using FFTInPlaceFuncPtr = void(*)(af::array&, const double);
using FFTFuncPtr = af::array(*)(const af::array&, const double);
using FFTFunc = std::function<af::array(const af::array&, const double)>;

template <typename In, typename Out>
class FFTBaseBlock: public ArrayFireBlock
{
    public:
        using InType = In;
        using OutType = Out;
        using Class = FFTBaseBlock<In, Out>;

        FFTBaseBlock(
            size_t numBins,
            double norm,
            size_t nchans,
            const std::string& blockRegistryPath
        ):
            ArrayFireBlock(),
            _numBins(numBins),
            _norm(0.0), // Set with class setter
            _nchans(nchans)
        {
            if(!isPowerOfTwo(numBins))
            {
                auto& logger = Poco::Logger::get(blockRegistryPath);
                poco_warning(
                    logger,
                    "This block is most efficient when "
                    "numBins is a power of 2.");
            }

            static const Pothos::DType inDType(typeid(InType));
            static const Pothos::DType outDType(typeid(OutType));

            for(size_t chan = 0; chan < _nchans; ++chan)
            {
                this->setupInput(chan, inDType);
                this->setupOutput(chan, outDType, this->getPortDomain());
            }

            this->registerProbe(
                "getNormalizationFactor",
                "normalizationFactorChanged",
                "setNormalizationFactor");

            this->setNormalizationFactor(norm);

            this->registerCall(this, POTHOS_FCN_TUPLE(Class, getNormalizationFactor));
            this->registerCall(this, POTHOS_FCN_TUPLE(Class, setNormalizationFactor));
        }

        virtual ~FFTBaseBlock() = default;

        // Custom output buffer manager with slabs large enough for the FFT
        // result.
        Pothos::BufferManager::Sptr getOutputBufferManager(const std::string&, const std::string&) override
        {
            Pothos::BufferManagerArgs args;
            args.bufferSize = _numBins*sizeof(OutType);
            return Pothos::BufferManager::make("generic", args);
        }

        double getNormalizationFactor() const
        {
            return _norm;
        }

        void setNormalizationFactor(double norm)
        {
            _norm = norm;

            this->emitSignal("normalizationFactorChanged", _norm);
        }

    protected:
        size_t _numBins;
        double _norm;
        size_t _nchans;
};

template <typename T>
class FFTBlock: public FFTBaseBlock<T,T>
{
    public:
        FFTBlock(
            FFTInPlaceFuncPtr func,
            size_t numBins,
            double norm,
            size_t nchans
        ):
            FFTBaseBlock<T,T>(numBins, norm, nchans, fftBlockPath),
            _func(func)
        {
        }

        virtual ~FFTBlock() = default;

        void work() override
        {
            auto elems = this->workInfo().minElements;
            if(0 == elems)
            {
                return;
            }

            auto afArray = this->getNumberedInputPortsAs2DAfArray();

            /*
             * Before ArrayFire 3.6, gfor was not thread-safe, as some
             * internal bookkeeping was stored globally. As of ArrayFire 3.6,
             * all of this stuff is thread-local, so we can take advantage of
             * it.
             */
            #if AF_CONFIG_PER_THREAD
            gfor(size_t chan, this->_nchans)
            #else
            for(size_t chan = 0; chan < this->_nchans; ++chan)
            #endif
            {
                af::array row(afArray.row(chan));
                _func(row, this->_norm);
                afArray(row) = row;
            }

            this->post2DAfArrayToNumberedOutputPorts(afArray);
        }

    private:
        FFTInPlaceFuncPtr _func;
};

template <typename In, typename Out>
class RFFTBlock: public FFTBaseBlock<In,Out>
{
    public:
        RFFTBlock(
            const FFTFunc& func,
            size_t numBins,
            double norm,
            size_t nchans
        ):
            FFTBaseBlock<In,Out>(numBins, norm, nchans, rfftBlockPath),
            _func(func)
        {
        }

        virtual ~RFFTBlock() = default;

        void work() override
        {
            auto elems = this->workInfo().minElements;
            if(0 == elems)
            {
                return;
            }

            auto afArray = this->getNumberedInputPortsAs2DAfArray();

            /*
             * Before ArrayFire 3.6, gfor was not thread-safe, as some
             * internal bookkeeping was stored globally. As of ArrayFire 3.6,
             * all of this stuff is thread-local, so we can take advantage of
             * it.
             */
            #if AF_CONFIG_PER_THREAD
            gfor(size_t chan, this->_nchans)
            #else
            for(size_t chan = 0; chan < this->_nchans; ++chan)
            #endif
            {
                afArray.row(chan) = _func(afArray.row(chan), this->_norm);
            }

            this->post2DAfArrayToNumberedOutputPorts(afArray);
        }

    private:
        FFTFunc _func;
};

//
// Factories
//

static Pothos::Block* makeFFT(
    const Pothos::DType& dtype,
    size_t numBins,
    double norm,
    size_t numChannels,
    bool inverse)
{
    FFTInPlaceFuncPtr func = inverse ? &af::ifftInPlace : &af::fftInPlace;

    #define ifTypeDeclareFactory(T) \
        if(Pothos::DType::fromDType(dtype, 1) == Pothos::DType(typeid(T))) \
            return new FFTBlock<T>(func, numBins, norm, numChannels);

    ifTypeDeclareFactory(std::complex<float>)
    ifTypeDeclareFactory(std::complex<double>)
    #undef ifTypeDeclareFactory

    throw Pothos::InvalidArgumentException(
              "Unsupported type",
              dtype.name());
}

static Pothos::Block* makeRFFT(
    const Pothos::DType& dtype,
    size_t numBins,
    double norm,
    size_t numChannels,
    bool inverse)
{
    auto getC2RFunc = [&numBins]() -> FFTFunc
    {
        // We need to point to a specific af::fftC2R overload.
        using fftC2RFuncPtr = af::array(*)(const af::array&, bool, const double);
        fftC2RFuncPtr func = &af::fftC2R<1>;

        FFTFunc ret(std::bind(
                        func,
                        std::placeholders::_1,
                        (1 == (numBins % 2)),
                        std::placeholders::_2));
        return ret;
    };

    FFTFunc func = inverse ? getC2RFunc() : FFTFuncPtr(&af::fftR2C<1>);

    #define ifTypeDeclareFactory(T) \
        if(Pothos::DType::fromDType(dtype, 1) == Pothos::DType(typeid(T))) \
        { \
            if(inverse) return new RFFTBlock<T,std::complex<T>>(func,numBins,norm,numChannels); \
            else        return new RFFTBlock<std::complex<T>,T>(func,numBins,norm,numChannels); \
        }

    ifTypeDeclareFactory(float)
    ifTypeDeclareFactory(double)
    #undef ifTypeDeclareFactory

    throw Pothos::InvalidArgumentException(
              "Unsupported type",
              dtype.name());
}

//
// Block registries
//

/*
 * |PothosDoc FFT
 *
 * Calculates the 1-dimensional FFT of all input streams.
 *
 * Calls <b>af::fftInPlace</b> or <b>af::ifftInPlace</b> on all inputs.
 * This block computes all outputs in parallel, using one of the following
 * implementations by priority (based on availability of hardware and
 * underlying libraries).
 * <ol>
 * <li>CUDA (if GPU present)</li>
 * <li>OpenCL (if GPU present)</li>
 * <li>Standard C++ (if no GPU present)</li>
 * </ol>
 *
 * |category /ArrayFire/Signal
 * |keywords array signal fft ifft fourier
 * |factory /arrayfire/signal/fft(dtype,numBins,norm,numChannels,inverse)
 * |setter setNormalizationFactor(norm)
 *
 * |param dtype[Data Type] The output's data type.
 * |widget DTypeChooser(cfloat=1)
 * |default "complex_float64"
 * |preview disable
 *
 * |param numBins[Num FFT Bins] The number of bins per FFT.
 * |default 1024
 * |option 512
 * |option 1024
 * |option 2048
 * |option 4096
 * |widget ComboBox(editable=true)
 * |preview enable
 *
 * |param norm[Normalization Factor]
 * |widget DoubleSpinBox(minimum=0.0)
 * |default 1.0
 * |preview enable
 *
 * |param numChannels[Num Channels] The number of channels.
 * |widget SpinBox(minimum=1)
 * |default 1
 * |preview disable
 *
 * |param inverse[Inverse?]
 * |widget ToggleSwitch()
 * |preview enable
 * |default false
 */
static Pothos::BlockRegistry registerFFT(
    fftBlockPath,
    Pothos::Callable(&makeFFT));

/*
 * |PothosDoc Real FFT
 *
 * Calculates the 1-dimensional real FFT of all input streams.
 *
 * Calls <b>af::fftR2C\<1\></b> or <b>af::fftC2R\<1\></b> on all inputs.
 * This block computes all outputs in parallel, using one of the following
 * implementations by priority (based on availability of hardware and
 * underlying libraries).
 * <ol>
 * <li>CUDA (if GPU present)</li>
 * <li>OpenCL (if GPU present)</li>
 * <li>Standard C++ (if no GPU present)</li>
 * </ol>
 *
 * |category /ArrayFire/Signal
 * |keywords array signal fft ifft rfft fourier
 * |factory /arrayfire/signal/rfft(dtype,numBins,norm,numChannels,inverse)
 * |setter setNormalizationFactor(norm)
 *
 * |param dtype[Data Type] The floating-type underlying the input types.
 * |widget DTypeChooser(float=1)
 * |default "float64"
 * |preview disable
 *
 * |param numBins[Num FFT Bins] The number of bins per FFT.
 * |default 1024
 * |option 512
 * |option 1024
 * |option 2048
 * |option 4096
 * |widget ComboBox(editable=true)
 * |preview enable
 *
 * |param norm[Normalization Factor]
 * |widget DoubleSpinBox(minimum=0.0)
 * |default 1.0
 * |preview enable
 *
 * |param numChannels[Num Channels] The number of channels.
 * |widget SpinBox(minimum=1)
 * |default 1
 * |preview disable
 *
 * |param inverse[Inverse?]
 * |widget ToggleSwitch()
 * |preview enable
 * |default false
 */
static Pothos::BlockRegistry registerRFFT(
    rfftBlockPath,
    Pothos::Callable(&makeRFFT));
