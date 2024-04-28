#pragma once

#include <google/protobuf/text_format.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <fstream>
#include <memory>
#include <map>
#include <string.h>

#define M_SHEET_KEY_INIT(key) \
if (typeid(key) != typeid(std::string)) memset(&key, 0, sizeof(key));

namespace sheetcfg {
    template<typename KEY1 = int64_t, typename KEY2 = int64_t, typename KEY3 = int64_t, typename KEY4 = int64_t>
    struct SheetKey {
    public:
        typedef KEY1 _KEY1;
        typedef KEY2 _KEY2;
        typedef KEY3 _KEY3;
        typedef KEY4 _KEY4;

        SheetKey() {
            M_SHEET_KEY_INIT(this->key1);
            M_SHEET_KEY_INIT(this->key2);
            M_SHEET_KEY_INIT(this->key3);
            M_SHEET_KEY_INIT(this->key4);
        }

        SheetKey(const KEY1& key1) {
            this->key1 = key1;
            M_SHEET_KEY_INIT(this->key2);
            M_SHEET_KEY_INIT(this->key3);
            M_SHEET_KEY_INIT(this->key4);
        }

        SheetKey(const KEY1& key1, const KEY2& key2) {
            this->key1 = key1;
            this->key2 = key2;
            M_SHEET_KEY_INIT(this->key3);
            M_SHEET_KEY_INIT(this->key4);
        }

        SheetKey(const KEY1& key1, const KEY2& key2, const KEY3& key3) {
            this->key1 = key1;
            this->key2 = key2;
            this->key3 = key3;
            M_SHEET_KEY_INIT(this->key4);
        }

        SheetKey(const KEY1& key1, const KEY2& key2, const KEY3& key3, const KEY4& key4) {
            this->key1 = key1;
            this->key2 = key2;
            this->key3 = key3;
            this->key4 = key4;
        }

        bool operator<(const SheetKey<KEY1, KEY2, KEY3, KEY4>& other) const {
            if (this->key1 < other.key1)
            {
                return true;
            }
            if (this->key1 > other.key1)
            {
                return false;
            }

            if (this->key2 < other.key2)
            {
                return true;
            }
            if (this->key2 > other.key2)
            {
                return false;
            }

            if (this->key3 < other.key3)
            {
                return true;
            }
            if (this->key3 > other.key3)
            {
                return false;
            }

            if (this->key4 < other.key4)
            {
                return true;
            }
            if (this->key4 > other.key4)
            {
                return false;
            }

            return false;
        }

        KEY1 key1;
        KEY2 key2;
        KEY3 key3;
        KEY4 key4;
    };

    template<typename KEY, typename NEW_ITEM_TYPE, typename ITEM_TYPE, typename SHEET_TYPE>
    struct SheetReader {
    public:
        virtual bool Load(const char* file_path) {
            std::ifstream ifs(file_path, std::ifstream::in);
            if (!ifs) {
                printf("failed to open proto file:%s\n", file_path);
                return false;
            }

            auto newSheet = std::make_shared<SHEET_TYPE>();
            auto newItemMap = std::make_shared<std::map<KEY, std::shared_ptr<NEW_ITEM_TYPE>>>();

            google::protobuf::io::IstreamInputStream inputStream(&ifs);
            if (!google::protobuf::TextFormat::Parse(&inputStream, newSheet.get())) {
                printf("failed to parser proto file:%s\n", file_path);
                return false;
            }

            for (int idx = 0; idx < newSheet->items_size(); ++idx) {
                std::shared_ptr<NEW_ITEM_TYPE> ptr = std::make_shared<NEW_ITEM_TYPE>();
                KEY key;
                if (!parser(key, *ptr.get(), *(newSheet->mutable_items(idx)))) {
                    printf("failed to parser item:%s|%s\n", file_path, newSheet->mutable_items(idx)->ShortDebugString().c_str());
                    return false;
                }
                (*newItemMap)[key] = ptr;
            }

            mSheet = newSheet;
            mItemMap = newItemMap;
            return true;
        }

        std::shared_ptr<NEW_ITEM_TYPE> GetItem(const KEY& key) const {
            auto iter = mItemMap->find(key);
            if (iter == mItemMap->end()) {
                return nullptr;
            }

            return iter->second;
        }

        std::shared_ptr<NEW_ITEM_TYPE> GetItem(const typename KEY::_KEY1& key1) const {
            return GetItem(SheetKey<typename KEY::_KEY1>(key1));
        }

        std::shared_ptr<NEW_ITEM_TYPE> GetItem(const typename KEY::_KEY1& key1, const typename KEY::_KEY2& key2) const {
            return GetItem(SheetKey<typename KEY::_KEY1, typename KEY::_KEY2>(key1, key2));
        }

        std::shared_ptr<NEW_ITEM_TYPE> GetItem(const typename KEY::_KEY1& key1, const typename KEY::_KEY2& key2, const typename KEY::_KEY3& key3) {
            return GetItem(SheetKey<typename KEY::_KEY1, typename KEY::_KEY2, typename KEY::_KEY3>(key1, key2, key3));
        }

        std::shared_ptr<NEW_ITEM_TYPE> GetItem(const typename KEY::_KEY1& key1, const typename KEY::_KEY2& key2, const typename KEY::_KEY3& key3, const typename KEY::_KEY4& key4) {
            return GetItem(SheetKey<typename KEY::_KEY1, typename KEY::_KEY2, typename KEY::_KEY3, typename KEY::_KEY4>(key1, key2, key3, key4));
        }

        std::shared_ptr<SHEET_TYPE> GetSheet() const {
            return mSheet;
        }

        std::shared_ptr<std::map<KEY, std::shared_ptr<NEW_ITEM_TYPE>>> GetItemMap() const {
            return mItemMap;
        }

        virtual bool check() const { return true; }

    protected:
        // 解析器实现
        virtual bool parser(KEY& key, NEW_ITEM_TYPE& new_item, ITEM_TYPE& item) = 0;

    protected:
        std::shared_ptr<std::map<KEY, std::shared_ptr<NEW_ITEM_TYPE>>> mItemMap;
        std::shared_ptr<SHEET_TYPE> mSheet;
    };
}

