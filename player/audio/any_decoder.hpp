// PT: Apagamento de tipo sobre o concept Decoder: guarda qualquer adapter
//     que satisfaça Decoder e despacha por uma vtable de funções ESTÁTICAS
//     (zero virtual por chamada no hot path — o indirection é de função, não
//     de método). Satisfaz o próprio concept (comporta-se como Decoder).
// EN: Type erasure over the Decoder concept: holds any adapter satisfying
//     Decoder and dispatches through a STATIC function table (no per-call
//     virtual overhead — indirection is a function pointer, not a method).
//     Satisfies the concept itself (behaves as a Decoder).
// ES: Borrado de tipo sobre el concept Decoder: guarda cualquier adapter que
//     satisfaga Decoder y despacha por una vtable de funciones ESTÁTICAS
//     (sin virtual por llamada en el hot path). Satisface el propio concept.
// ZH: 基于 Decoder concept 的类型擦除：保存任何满足 Decoder 的适配器，通过
//     静态函数表分发（热路径上无 virtual 开销 —— 间接层是函数指针而非方法）。
//     自身也满足该 concept（表现为一个 Decoder）。
#pragma once

#include "audio/decoder.hpp"

#include <chrono>
#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <utility>

namespace audio
{

class AnyDecoder
{
      public:
        AnyDecoder() = default;

        // PT: Constrói a partir de qualquer Decoder (MaDecoder, WebmDecoder...).
        // EN: Constructs from any Decoder (MaDecoder, WebmDecoder...).
        // ES: Construye a partir de cualquier Decoder (MaDecoder, WebmDecoder...).
        // ZH: 从任何 Decoder 构造（MaDecoder、WebmDecoder……）。
        template <Decoder D>
        AnyDecoder(D d) // NOLINT: conversão implícita intencional / intentional implicit conversion / 刻意的隐式转换
        {
                emplace<D>(std::move(d));
        }

        ~AnyDecoder()
        {
                reset();
        }
        AnyDecoder(AnyDecoder &&other) noexcept : vtbl_(std::exchange(other.vtbl_, nullptr)),
                                                  ptr_(std::exchange(other.ptr_, nullptr))
        {
        }
        AnyDecoder &operator=(AnyDecoder &&other) noexcept
        {
                if (this != &other)
                {
                        reset();
                        vtbl_ = std::exchange(other.vtbl_, nullptr);
                        ptr_ = std::exchange(other.ptr_, nullptr);
                }
                return *this;
        }
        AnyDecoder(const AnyDecoder &) = delete;
        AnyDecoder &operator=(const AnyDecoder &) = delete;

        [[nodiscard]] explicit operator bool() const
        {
                return ptr_ != nullptr;
        }

        [[nodiscard]] bool open(const std::string &path, int target_sample_rate)
        {
                return vtbl_ && vtbl_->open(ptr_, path, target_sample_rate);
        }
        [[nodiscard]] std::size_t read(std::span<float> out)
        {
                return vtbl_ ? vtbl_->read(ptr_, out) : 0;
        }
        [[nodiscard]] bool seek(std::chrono::microseconds at)
        {
                return vtbl_ && vtbl_->seek(ptr_, at);
        }
        [[nodiscard]] TrackInfo info() const
        {
                return vtbl_ ? vtbl_->info(ptr_) : TrackInfo{};
        }
        [[nodiscard]] bool at_end() const
        {
                return !vtbl_ || vtbl_->at_end(ptr_);
        }

      private:
        struct Vtbl
        {
                bool (*open)(void *, const std::string &, int);
                std::size_t (*read)(void *, std::span<float>);
                bool (*seek)(void *, std::chrono::microseconds);
                TrackInfo (*info)(const void *);
                bool (*at_end)(const void *);
                void (*destroy)(void *);
        };

        void reset() noexcept
        {
                if (vtbl_)
                        vtbl_->destroy(ptr_);
                vtbl_ = nullptr;
                ptr_ = nullptr;
        }

        template <Decoder D>
        void emplace(D d)
        {
                auto *p = new D(std::move(d));
                ptr_ = p;
                vtbl_ = &table<D>();
        }

        // PT: Uma vtable por tipo D: as funções fazem cast de volta e chamam
        //     o adapter direto — o compiler inlinha o corpo do adapter.
        // EN: One vtable per D: the functions cast back and call the adapter
        //     directly — the compiler inlines the adapter body.
        // ES: Una vtable por tipo D: las funciones hacen cast de vuelta y
        //     llaman al adapter directo — el compilador inlinea el cuerpo.
        // ZH: 每个 D 类型一张 vtable：函数转回具体类型直接调用适配器 —— 编译器
        //     会内联适配器本体。
        template <Decoder D>
        static const Vtbl &table()
        {
                static const Vtbl t{
                    [](void *p, const std::string &path, int rate) {
                            return static_cast<D *>(p)->open(path, rate);
                    },
                    [](void *p, std::span<float> out) { return static_cast<D *>(p)->read(out); },
                    [](void *p, std::chrono::microseconds at) { return static_cast<D *>(p)->seek(at); },
                    [](const void *p) { return static_cast<const D *>(p)->info(); },
                    [](const void *p) { return static_cast<const D *>(p)->at_end(); },
                    [](void *p) { delete static_cast<D *>(p); },
                };
                return t;
        }

        const Vtbl *vtbl_ = nullptr;
        void *ptr_ = nullptr;
};

static_assert(Decoder<AnyDecoder>, "AnyDecoder must satisfy the Decoder concept");

} // namespace audio
