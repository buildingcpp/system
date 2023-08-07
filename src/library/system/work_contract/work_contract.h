#pragma once

#include <atomic>
#include <cstdint>


namespace bcpp::system
{

    enum class work_contract_mode : std::uint32_t;
    
    template <work_contract_mode> 
    class work_contract_group;


    template <work_contract_mode T>
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

        friend class work_contract_group<T>;

        work_contract
        (
            work_contract_group<T> *, 
            std::shared_ptr<typename work_contract_group<T>::surrender_token>,
            id_type
        );

        work_contract_group<T> *   owner_{};

        std::shared_ptr<typename work_contract_group<T>::surrender_token> surrenderToken_;

        id_type                 id_{};

    }; // class work_contract


    using waitable_work_contract = work_contract<work_contract_mode::waitable>;
    using non_waitable_work_contract = work_contract<work_contract_mode::waitable>;

    using basic_work_contract = non_waitable_work_contract;

} // namespace bcpp::system

#include "./work_contract_group.h"


//=============================================================================
template <bcpp::system::work_contract_mode T>
inline bcpp::system::work_contract<T>::work_contract
(
    work_contract_group<T> * owner,
    std::shared_ptr<typename work_contract_group<T>::surrender_token> surrenderToken, 
    id_type id
):
    owner_(owner),
    surrenderToken_(surrenderToken),
    id_(id)
{
}


//=============================================================================
template <bcpp::system::work_contract_mode T>
inline bcpp::system::work_contract<T>::work_contract
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
template <bcpp::system::work_contract_mode T>
inline auto bcpp::system::work_contract<T>::operator =
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
template <bcpp::system::work_contract_mode T>
inline bcpp::system::work_contract<T>::~work_contract
(
)
{
    surrender();
}


//=============================================================================
template <bcpp::system::work_contract_mode T>
inline auto bcpp::system::work_contract<T>::get_id
(
) const -> id_type
{
    return id_;
}


//=============================================================================
template <bcpp::system::work_contract_mode T>
inline void bcpp::system::work_contract<T>::invoke
(
)
{
    owner_->invoke(*this);
}


//=============================================================================
template <bcpp::system::work_contract_mode T>
inline void bcpp::system::work_contract<T>::operator()
(
)
{
    invoke();
}


//=============================================================================
template <bcpp::system::work_contract_mode T>
inline bool bcpp::system::work_contract<T>::surrender
(
)
{
    owner_ = {};
    return (surrenderToken_) ? surrenderToken_->invoke(*this) : false;
}


//=============================================================================
template <bcpp::system::work_contract_mode T>
inline bool bcpp::system::work_contract<T>::is_valid
(
) const
{
    return (owner_ != nullptr);
}


//=============================================================================
template <bcpp::system::work_contract_mode T>
inline bcpp::system::work_contract<T>::operator bool
(
) const
{
    return is_valid();
}
