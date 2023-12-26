#pragma once

#include "./work_contract_mode.h"
#include <include/non_copyable.h>

#include <atomic>
#include <cstdint>
#include <utility>


namespace bcpp::system
{

    namespace internal
    {
        template <work_contract_mode T>
        class sub_work_contract_group;


        template <work_contract_mode T>
        class work_contract :
            non_copyable
        {
        public:

            using id_type = std::uint32_t;

            work_contract() = default;
            ~work_contract();

            work_contract(work_contract &&);
            work_contract & operator = (work_contract &&);

            void schedule();

            bool release();

            bool is_valid() const;

            explicit operator bool() const;


        private:

            friend class sub_work_contract_group<T>;

            work_contract
            (
                sub_work_contract_group<T> *, 
                std::shared_ptr<typename sub_work_contract_group<T>::release_token>,
                id_type
            );

            id_type get_id() const;

            sub_work_contract_group<T> *   owner_{};

            std::shared_ptr<typename sub_work_contract_group<T>::release_token> releaseToken_;

            id_type                 id_{};

        }; // class work_contract

    } // namespace internal

    using work_contract = internal::work_contract<internal::work_contract_mode::non_blocking>;
    using blocking_work_contract = internal::work_contract<internal::work_contract_mode::blocking>;

} // namespace bcpp::system


#include "./sub_work_contract_group.h"


//=============================================================================
template <bcpp::system::internal::work_contract_mode T>
inline bcpp::system::internal::work_contract<T>::work_contract
(
    sub_work_contract_group<T> * owner,
    std::shared_ptr<typename sub_work_contract_group<T>::release_token> releaseToken, 
    id_type id
):
    owner_(owner),
    releaseToken_(releaseToken),
    id_(id)
{
}


//=============================================================================
template <bcpp::system::internal::work_contract_mode T>
inline bcpp::system::internal::work_contract<T>::work_contract
(
    work_contract && other
):
    owner_(other.owner_),
    releaseToken_(other.releaseToken_),
    id_(other.id_)
{
    other.owner_ = {};
    other.id_ = {};
    other.releaseToken_ = {};
}

    
//=============================================================================
template <bcpp::system::internal::work_contract_mode T>
inline auto bcpp::system::internal::work_contract<T>::operator =
(
    work_contract && other
) -> work_contract &
{
    release();

    owner_ = other.owner_;
    id_ = other.id_;
    releaseToken_ = other.releaseToken_;
    
    other.owner_ = {};
    other.id_ = {};
    other.releaseToken_ = {};
    return *this;
}


//=============================================================================
template <bcpp::system::internal::work_contract_mode T>
inline bcpp::system::internal::work_contract<T>::~work_contract
(
)
{
    release();
}


//=============================================================================
template <bcpp::system::internal::work_contract_mode T>
inline auto bcpp::system::internal::work_contract<T>::get_id
(
) const -> id_type
{
    return id_;
}


//=============================================================================
template <bcpp::system::internal::work_contract_mode T>
inline void bcpp::system::internal::work_contract<T>::schedule
(
)
{
    owner_->schedule(*this);
}


//=============================================================================
template <bcpp::system::internal::work_contract_mode T>
inline bool bcpp::system::internal::work_contract<T>::release
(
)
{
    if (auto releaseToken = std::exchange(releaseToken_, nullptr); releaseToken)
    {
        releaseToken->schedule(*this);
        return true;
    }
    return false;
}


//=============================================================================
template <bcpp::system::internal::work_contract_mode T>
inline bool bcpp::system::internal::work_contract<T>::is_valid
(
) const
{
    return ((releaseToken_) && (releaseToken_->is_valid()));
}


//=============================================================================
template <bcpp::system::internal::work_contract_mode T>
inline bcpp::system::internal::work_contract<T>::operator bool
(
) const
{
    return is_valid();
}
