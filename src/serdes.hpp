/*******************************************************************************
  Copyright (c) 2016, Manuel Freiberger
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

  * Redistributions of source code must retain the above copyright notice, this
    list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*******************************************************************************/

#ifndef LOG11_SERDES_HPP
#define LOG11_SERDES_HPP

#include "ringbuffer.hpp"

#include <cstddef>
#include <utility>


template <typename... T>
struct TypeList {};

class Visitor
{
public:
    virtual
    ~Visitor() {}

    virtual void visit(int value) = 0;
    virtual void visit(float value) = 0;
};

class SerdesBase
{
public:
    virtual
    ~SerdesBase() {}

    virtual
    std::size_t numArguments() const noexcept = 0;

    virtual
    void apply(RingBuffer& buffer, RingBuffer::ByteRange range,
               std::size_t index, Visitor& visitor) = 0;
};

template <typename... T>
class Serdes : public SerdesBase
{
public:
    static
    SerdesBase* instance()
    {
        static Serdes serdes;
        return &serdes;
    }

    static
    std::size_t requiredSize(const T&... args)
    {
        return doRequiredSize(args...);
    }

    template <typename... TArgs>
    static
    void serialize(RingBuffer& buffer, RingBuffer::ByteRange range,
                   TArgs&&... args)
    {
        doSerialize(buffer, range, std::forward<TArgs&&>(args)...);
    }

    virtual
    std::size_t numArguments() const noexcept override
    {
        return sizeof...(T);
    }

    virtual
    void apply(RingBuffer& buffer, RingBuffer::ByteRange range,
               std::size_t index, Visitor& visitor) override
    {
        return index < numArguments() ? doApply(TypeList<T...>(), buffer, range, index, visitor)
                                      : (void)0;
    }

private:
    template <typename TArg, typename... TArgs>
    static
    std::size_t doRequiredSize(const TArg& arg, const TArgs&... args)
    {
        return sizeof(arg) + doRequiredSize(args...);
    }

    static
    std::size_t doRequiredSize()
    {
        return 0;
    }

    template <typename TArg, typename... TArgs>
    static
    void doSerialize(RingBuffer& buffer, RingBuffer::ByteRange range,
                     TArg&& arg, TArgs&&... args)
    {
        range = buffer.write((TArg*)&arg, range, sizeof(TArg));
        doSerialize(buffer, range, std::forward<TArgs&&>(args)...);
    }

    static
    void doSerialize(RingBuffer&, RingBuffer::ByteRange)
    {
    }

    template <typename THead, typename... TTail>
    static
    void doApply(TypeList<THead, TTail...>,
                 RingBuffer& buffer, RingBuffer::ByteRange range,
                 std::size_t argIndex, Visitor& visitor)
    {
        if (range.length)
        {
            return argIndex == 0
                    ? extractAndApply<THead>(buffer, range, visitor)
                    : doApply(TypeList<TTail...>(),
                              buffer, advance<THead>(buffer, range),
                              argIndex - 1, visitor);
        }
    }

    static
    void doApply(TypeList<>,
                 RingBuffer& /*buffer*/, const RingBuffer::ByteRange& /*range*/,
                 std::size_t /*argIndex*/, Visitor& /*visitor*/)
    {
    }

    template <typename TType>
    static
    void extractAndApply(RingBuffer& buffer, RingBuffer::ByteRange range,
                         Visitor& visitor)
    {
        if (sizeof(TType) <= range.length)
        {
            TType temp;
            buffer.read(range, &temp, sizeof(TType));
            visitor.visit(temp);
        }
    }

    template <typename TType>
    static
    RingBuffer::ByteRange advance(RingBuffer&, const RingBuffer::ByteRange& range)
    {
        return RingBuffer::ByteRange(
                    range.begin + sizeof(TType),
                    range.length > sizeof(TType) ? range.length - sizeof(TType) : 0);
    }
};

#endif // LOG11_SERDES_HPP