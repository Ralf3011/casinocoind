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
    2017-06-30  ajochems        Refactored for casinocoin
*/
//==============================================================================

#include <BeastConfig.h>
#include <casinocoin/app/main/Application.h>
#include <casinocoin/rpc/RPCHandler.h>
#include <casinocoin/rpc/impl/Tuning.h>
#include <casinocoin/rpc/impl/Handler.h>
#include <casinocoin/app/main/Application.h>
#include <casinocoin/app/ledger/LedgerMaster.h>
#include <casinocoin/app/misc/NetworkOPs.h>
#include <casinocoin/basics/contract.h>
#include <casinocoin/basics/Log.h>
#include <casinocoin/core/Config.h>
#include <casinocoin/core/JobQueue.h>
#include <casinocoin/json/Object.h>
#include <casinocoin/json/to_string.h>
#include <casinocoin/net/InfoSub.h>
#include <casinocoin/net/RPCErr.h>
#include <casinocoin/protocol/JsonFields.h>
#include <casinocoin/resource/Fees.h>
#include <casinocoin/rpc/Role.h>
#include <casinocoin/resource/Fees.h>

namespace casinocoin {
namespace RPC {

namespace {

/**
   This code is called from both the HTTP RPC handler and Websockets.

   The form of the Json returned is somewhat different between the two services.

   HTML:
     Success:
        {
           "result" : {
              "ledger" : {
                 "accepted" : false,
                 "transaction_hash" : "..."
              },
              "ledger_index" : 10300865,
              "validated" : false,
              "status" : "success"  # Status is inside the result.
           }
        }

     Failure:
        {
           "result" : {
              "error" : "noNetwork",
              "error_code" : 16,
              "error_message" : "Not synced to Casinocoin network.",
              "request" : {
                 "command" : "ledger",
                 "ledger_index" : 10300865
              },
              "status" : "error"
           }
        }

   Websocket:
     Success:
        {
           "result" : {
              "ledger" : {
                 "accepted" : false,
                 "transaction_hash" : "..."
              },
              "ledger_index" : 10300865,
              "validated" : false
           }
           "type": "response",
           "status": "success",   # Status is OUTside the result!
           "id": "client's ID",   # Optional
           "warning": 3.14        # Optional
        }

     Failure:
        {
          "error" : "noNetwork",
          "error_code" : 16,
          "error_message" : "Not synced to Casinocoin network.",
          "request" : {
             "command" : "ledger",
             "ledger_index" : 10300865
          },
          "type": "response",
          "status" : "error",
          "id": "client's ID"   # Optional
        }

 */

error_code_i fillHandler (Context& context,
                          Handler const * & result)
{
    if (! isUnlimited (context.role))
    {
        // VFALCO NOTE Should we also add up the jtRPC jobs?
        //
        int jc = context.app.getJobQueue ().getJobCountGE (jtCLIENT);
        if (jc > Tuning::maxJobQueueClients)
        {
            JLOG (context.j.debug()) << "Too busy for command: " << jc;
            return rpcTOO_BUSY;
        }
    }

    if (!context.params.isMember(jss::command) && !context.params.isMember(jss::method))
        return rpcCOMMAND_MISSING;
    if (context.params.isMember(jss::command) && context.params.isMember(jss::method))
    {
        if (context.params[jss::command].asString() !=
            context.params[jss::method].asString())
            return rpcUNKNOWN_COMMAND;
    }

    std::string strCommand  = context.params.isMember(jss::command) ?
                              context.params[jss::command].asString() :
                              context.params[jss::method].asString();

    JLOG (context.j.trace()) << "COMMAND:" << strCommand;
    JLOG (context.j.trace()) << "REQUEST:" << context.params;
    auto handler = getHandler(strCommand);

    if (!handler)
        return rpcUNKNOWN_COMMAND;

    if (handler->role_ == Role::ADMIN && context.role != Role::ADMIN)
        return rpcNO_PERMISSION;

    if ((handler->condition_ & NEEDS_NETWORK_CONNECTION) &&
        (context.netOps.getOperatingMode () < NetworkOPs::omSYNCING))
    {
        JLOG (context.j.info())
            << "Insufficient network mode for RPC: "
            << context.netOps.strOperatingMode ();

        return rpcNO_NETWORK;
    }

    if (!context.app.config().standalone() &&
        handler->condition_ & NEEDS_CURRENT_LEDGER)
    {
        if (context.ledgerMaster.getValidatedLedgerAge () >
            Tuning::maxValidatedLedgerAge)
        {
            return rpcNO_CURRENT;
        }

        auto const cID = context.ledgerMaster.getCurrentLedgerIndex ();
        auto const vID = context.ledgerMaster.getValidLedgerIndex ();

        if (cID + 10 < vID)
        {
            JLOG (context.j.debug()) << "Current ledger ID(" << cID <<
                ") is less than validated ledger ID(" << vID << ")";
            return rpcNO_CURRENT;
        }
    }

    if ((handler->condition_ & NEEDS_CLOSED_LEDGER) &&
        !context.ledgerMaster.getClosedLedger ())
    {
        return rpcNO_CLOSED;
    }

    result = handler;
    return rpcSUCCESS;
}

template <class Object, class Method>
Status callMethod (
    Context& context, Method method, std::string const& name, Object& result)
{
    try
    {
        auto v = context.app.getJobQueue().makeLoadEvent(
            jtGENERIC, "cmd:" + name);
        return method (context, result);
    }
    catch (std::exception& e)
    {
        JLOG (context.j.info()) << "Caught throw: " << e.what ();

        if (context.loadType == Resource::feeReferenceRPC)
            context.loadType = Resource::feeExceptionRPC;

        inject_error (rpcINTERNAL, result);
        return rpcINTERNAL;
    }
}

template <class Method, class Object>
void getResult (
    Context& context, Method method, Object& object, std::string const& name)
{
    auto&& result = Json::addObject (object, jss::result);
    if (auto status = callMethod (context, method, name, result))
    {
        JLOG (context.j.debug()) << "rpcError: " << status.toString();
        result[jss::status] = jss::error;
        result[jss::request] = context.params;
    }
    else
    {
        result[jss::status] = jss::success;
    }
}

} // namespace

Status doCommand (
    RPC::Context& context, Json::Value& result)
{
    Handler const * handler = nullptr;
    if (auto error = fillHandler (context, handler))
    {
        inject_error (error, result);
        return error;
    }

    if (auto method = handler->valueMethod_)
    {
        if (! context.headers.user.empty() ||
            ! context.headers.forwardedFor.empty())
        {
            JLOG(context.j.debug()) << "start command: " << handler->name_ <<
                ", X-User: " << context.headers.user << ", X-Forwarded-For: " <<
                    context.headers.forwardedFor;

            auto ret = callMethod (context, method, handler->name_, result);

            JLOG(context.j.debug()) << "finish command: " << handler->name_ <<
                ", X-User: " << context.headers.user << ", X-Forwarded-For: " <<
                    context.headers.forwardedFor;

            return ret;
        }
        else
        {
            return callMethod (context, method, handler->name_, result);
        }
    }

    return rpcUNKNOWN_COMMAND;
}

Role roleRequired (std::string const& method)
{
    auto handler = RPC::getHandler(method);

    if (!handler)
        return Role::FORBID;

    return handler->role_;
}

} // RPC
} // casinocoin
