namespace std {
/*
.function("size", &MapType::size)
            .function("get", internal::MapAccess<MapType>::get)
            .function("set", internal::MapAccess<MapType>::set)
            .function("keys", internal::MapAccess<MapType>::keys)
*/
    template<class _K, class _V>
    class map {
     public:
        %extend {
            _V get(const _K& k){
                return (*self)[k];
            }
            void set(const _K& k, const _V& val) {
                (*self)[k] = val;
            }

            size_t size() {
                return self->size();
            }

            vector<_K> keys() {
                std::vector<_K> keys;
                keys.reserve(self->size());
                for (const auto& pair : (*self)) {
                    keys.push_back(pair.first);
                }
                return keys;
            }
        }
        
    };
}