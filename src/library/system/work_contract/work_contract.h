#pragma once

#include <atomic>
#include <cstdint>


namespace bcpp::system
{
    class work_contract_group;


    class work_contract
    {
    public:

        using id_type = std::uint32_t;

        work_contract() = default;
        ~work_contract();

        work_contract(work_contract &&);
        work_contract & operator = (work_contract &&);

        work_contract(work_contract const &) = delete;
        work_contract & operator = (work_contract const &) = delete;

        void operator()();

        void invoke();

        bool surrender();

        bool is_valid() const;

        explicit operator bool() const;

        id_type get_id() const;

    private:

        friend class work_contract_group;

        work_contract
        (
            work_contract_group *, 
            std::shared_ptr<typename work_contract_group::surrender_token>,
            id_type
        );

        work_contract_group *   owner_{};

        std::shared_ptr<typename work_contract_group::surrender_token> surrenderToken_;

        id_type                 id_{};

    }; // class work_contract

} // namespace bcpp::system

#include "./work_contract_group.h"


//=============================================================================
inline bcpp::system::work_contract::work_contract
(
    work_contract_group * owner,
    std::shared_ptr<typename work_contract_group::surrender_token> surrenderToken, 
    id_type id
):
    owner_(owner),
    surrenderToken_(surrenderToken),
    id_(id)
{
}


//=============================================================================
inline bcpp::system::work_contract::work_contract
(
    work_contract && other
):
    owner_(other.owner_),
    surrenderToken_(other.surrenderToken_),
    id_(other.id_)
{
    other.owner_ = {};
    other.id_ = {};
    other.surrenderToken_ = {};
}

    
//=============================================================================
inline auto bcpp::system::work_contract::operator =
(
    work_contract && other
) -> work_contract &
{
    surrender();

    owner_ = other.owner_;
    id_ = other.id_;
    surrenderToken_ = other.surrenderToken_;
    
    other.owner_ = {};
    other.id_ = {};
    other.surrenderToken_ = {};
    return *this;
}


//=============================================================================
inline bcpp::system::work_contract::~work_contract
(
)
{
    surrender();
}


//=============================================================================
inline auto bcpp::system::work_contract::get_id
(
) const -> id_type
{
    return id_;
}


//=============================================================================
inline void bcpp::system::work_contract::invoke
(
)
{
    owner_->invoke(*this);
}


//=============================================================================
inline void bcpp::system::work_contract::operator()
(
)
{
    invoke();
}


//=============================================================================
inline bool bcpp::system::work_contract::surrender
(
)
{
    if (auto surrenderToken = std::exchange(surrenderToken_, nullptr); surrenderToken)
    {
        surrenderToken->invoke(*this);
        return true;
    }
    return false;
}


//=============================================================================
inline bool bcpp::system::work_contract::is_valid
(
) const
{
    return ((bool)surrenderToken_);
}


//=============================================================================
inline bcpp::system::work_contract::operator bool
(
) const
{
    return is_valid();
}
