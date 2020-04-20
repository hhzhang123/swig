namespace std {
/*
tion("push_back", push_back)
.function("resize", resize)
.function("size", size)
.function("get", &internal::VectorAccess<VecType>::get)
.function("set", &internal::VectorAccess<VecType>::set)
*/
    template<class T>
    class vector {
    public:
        %extend {
            void push_back(const T& v) {
                self->push_back(v);
            }
            void resize(size_t size) {
                self->resize(size);
            }
            size_t size() {
                return self->size();
            }

            T get(size_t i) const {
                return (*self)[i];
            }

            bool set(size_t i, const T& v) {
                if (self->size() <= i || i < 0) {
                    return false;
                }
                (*self)[i] = v;
                return true;
            }
        }
    };
}