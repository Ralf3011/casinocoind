//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2012, 2013 Ripple Labs Inc.

    Permission to use, copy, modify, and/or distribute this software for any
    purpose  with  or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL ,  DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

//==============================================================================
/*
    2017-06-29  ajochems        Refactored for casinocoin
*/
//==============================================================================

#ifndef CASINOCOIN_APP_TX_TRANSACTOR_H_INCLUDED
#define CASINOCOIN_APP_TX_TRANSACTOR_H_INCLUDED

#include <casinocoin/app/tx/impl/ApplyContext.h>
#include <casinocoin/protocol/CSCAmount.h>
#include <casinocoin/beast/utility/Journal.h>
#include <boost/optional.hpp>

namespace casinocoin {

/** State information when preflighting a tx. */
struct PreflightContext
{
public:
    Application& app;
    STTx const& tx;
    Rules const rules;
    ApplyFlags flags;
    beast::Journal j;

    PreflightContext(Application& app_, STTx const& tx_,
        Rules const& rules_, ApplyFlags flags_,
            beast::Journal j_);

    PreflightContext& operator=(PreflightContext const&) = delete;
};

/** State information when determining if a tx is likely to claim a fee. */
struct PreclaimContext
{
public:
    Application& app;
    ReadView const& view;
    TER preflightResult;
    STTx const& tx;
    ApplyFlags flags;
    beast::Journal j;

    PreclaimContext(Application& app_, ReadView const& view_,
        TER preflightResult_, STTx const& tx_,
            ApplyFlags flags_, beast::Journal j_ = {})
        : app(app_)
        , view(view_)
        , preflightResult(preflightResult_)
        , tx(tx_)
        , flags(flags_)
        , j(j_)
    {
    }

    PreclaimContext& operator=(PreclaimContext const&) = delete;
};

struct TxConsequences;
struct PreflightResult;

class Transactor
{
protected:
    ApplyContext& ctx_;
    beast::Journal j_;

    AccountID     account_;
    CSCAmount     mFeeDue;
    CSCAmount     mPriorBalance;  // Balance before fees.
    CSCAmount     mSourceBalance; // Balance after fees.

public:
    /** Process the transaction. */
    std::pair<TER, bool>
    operator()();

    ApplyView&
    view()
    {
        return ctx_.view();
    }

    ApplyView const&
    view() const
    {
        return ctx_.view();
    }

    /////////////////////////////////////////////////////
    /*
    These static functions are called from invoke_preclaim<Tx>
    using name hiding to accomplish compile-time polymorphism,
    so derived classes can override for different or extra
    functionality. Use with care, as these are not really
    virtual and so don't have the compiler-time protection that
    comes with it.
    */

    static
    TER
    checkSeq (PreclaimContext const& ctx);

    static
    TER
    checkFee (PreclaimContext const& ctx, std::uint64_t baseFee);

    static
    TER
    checkSign (PreclaimContext const& ctx);

    static
    TER
    checkWLT (PreclaimContext const& ctx);

    static
    TER
    checkBlacklist (PreclaimContext const& ctx);

    static
    TER 
    checkWhitelist (PreclaimContext const& ctx);

    static
    TER 
    checkMemoSize (PreclaimContext const& ctx);

    // Returns the fee in fee units, not scaled for load.
    static
    std::uint64_t
    calculateBaseFee (
        PreclaimContext const& ctx);

    static
    bool
    affectsSubsequentTransactionAuth(STTx const& tx)
    {
        return false;
    }

    static
    CSCAmount
    calculateFeePaid(STTx const& tx);

    static
    CSCAmount
    calculateMaxSpend(STTx const& tx);

    static
    TER
    preclaim(PreclaimContext const &ctx)
    {
        // Most transactors do nothing
        // after checkSeq/Fee/Sign.
        return tesSUCCESS;
    }

    static
    TER
    isWLTCompliant(STAmount const& amount,
                   ConfigObjectEntry const& tokenConfig,
                   beast::Journal const& j);
    /////////////////////////////////////////////////////

protected:
    TER
    apply();

    explicit
    Transactor (ApplyContext& ctx);

    virtual void preCompute();

    virtual TER doApply () = 0;

private:
    void setSeq ();
    TER payFee ();
    void claimFee (CSCAmount& fee, TER terResult, std::vector<uint256> const& removedOffers);
    static TER checkSingleSign (PreclaimContext const& ctx);
    static TER checkMultiSign (PreclaimContext const& ctx);
};

/** Performs early sanity checks on the txid */
TER
preflight0(PreflightContext const& ctx);

/** Performs early sanity checks on the account and fee fields */
TER
preflight1 (PreflightContext const& ctx);

/** Checks whether the signature appears valid */
TER
preflight2 (PreflightContext const& ctx);

}

#endif
