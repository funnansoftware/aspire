module;
#include <string>
#include <unordered_map>
export module aspire.core.database;

import aspire.core.object;
import aspire.core.data;

export namespace aspire::core
{
    class Database : public aspire::core::Object
    {
    public:
        auto query(const std::string& x) const -> Data*
        {
            const auto it = data_.find(x);

            if (it != std::end(data_))
            {
                return it->second.get();
            }

            return nullptr;
        }

    protected:
        auto onStartup() -> void override
        {
            const auto data = getChildren<Data>();

            for (const auto& d : data)
            {
                data_[std::string{d->getName()}] = d;
            }
        }

    private:
        std::unordered_map<std::string, std::shared_ptr<Data>> data_;
    };
}