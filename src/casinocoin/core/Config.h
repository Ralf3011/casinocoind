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
    2018-01-18  jrojek          Added KYCSigners section
    2018-03-19  jrojek          Voting section moved to separate file
*/
//==============================================================================

#ifndef CASINOCOIN_CORE_CONFIG_H_INCLUDED
#define CASINOCOIN_CORE_CONFIG_H_INCLUDED

#include <casinocoin/basics/BasicConfig.h>
#include <casinocoin/basics/base_uint.h>
#include <casinocoin/protocol/SystemParameters.h> // VFALCO Breaks levelization
#include <casinocoin/beast/net/IPEndpoint.h>
#include <casinocoin/json/json_value.h>
#include <casinocoin/beast/utility/Journal.h>
#include <beast/core/detail/ci_char_traits.hpp>
#include <boost/asio/ip/tcp.hpp> // VFALCO FIX: This include should not be here
#include <boost/filesystem.hpp> // VFALCO FIX: This include should not be here
#include <boost/lexical_cast.hpp>
#include <boost/optional.hpp>
#include <cstdint>
#include <map>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <vector>

namespace casinocoin {

using namespace std::chrono_literals;

class Rules;

//------------------------------------------------------------------------------

enum SizedItemName
{
    siSweepInterval,
    siNodeCacheSize,
    siNodeCacheAge,
    siTreeCacheSize,
    siTreeCacheAge,
    siSLECacheSize,
    siSLECacheAge,
    siLedgerSize,
    siLedgerAge,
    siLedgerFetch,
    siHashNodeDBCache,
    siTxnDBCache,
    siLgrDBCache,
};

struct SizedItem
{
    SizedItemName   item;
    int             sizes[5];
};

//  This entire derived class is deprecated.
//  For new config information use the style implied
//  in the base class. For existing config information
//  try to refactor code to use the new style.
//
class Config : public BasicConfig
{
public:
    // Settings related to the configuration file location and directories
    static char const* const configFileName;
    static char const* const databaseDirName;
    static char const* const validatorsFileName;
    static char const* const votingFileName;
    static char const* const votableConfigFileName;

    /** Returns the full path and filename of the debug log file. */
    boost::filesystem::path getDebugLogFile () const;

    /** Returns the full path and filename of the entropy seed file. */
    boost::filesystem::path getEntropyFile () const;

    /** Returns the string for a given network id **/
    std::string getPeerNetworkString(uint32_t network);

private:
    boost::filesystem::path CONFIG_FILE;
    boost::filesystem::path CONFIG_DIR;
    boost::filesystem::path DEBUG_LOGFILE;

    void load ();
    bool
    loadSectionFromExternalPath(const std::string& sectionName,
                               boost::filesystem::path& filePath,
                               std::string& data, const std::string &defaultFilePath);
    beast::Journal j_;

    bool QUIET = false;          // Minimize logging verbosity.
    bool SILENT = false;         // No output to console after startup.
    /** Operate in stand-alone mode.

        In stand alone mode:

        - Peer connections are not attempted or accepted
        - The ledger is not advanced automatically.
        - If no ledger is loaded, the default ledger with the root
          account is created.
    */
    bool                        RUN_STANDALONE = false;

public:
    bool doImport = false;
    bool ELB_SUPPORT = false;

    std::vector<std::string>    IPS;                    // Peer IPs from casinocoind.cfg.
    std::vector<std::string>    IPS_FIXED;              // Fixed Peer IPs from casinocoind.cfg.
    std::vector<std::string>    SNTP_SERVERS;           // SNTP servers from casinocoind.cfg.

    enum StartUpType
    {
        FRESH,
        NORMAL,
        LOAD,
        LOAD_FILE,
        REPLAY,
        NETWORK
    };
    StartUpType                 START_UP = NORMAL;

    bool                        START_VALID = false;

    std::string                 START_LEDGER;

    // Network parameters
    int const                   TRANSACTION_FEE_BASE = 1000;   // The number of fee units a reference transaction costs

    // Note: The following parameters do not relate to the UNL or trust at all
    std::size_t                 NETWORK_QUORUM = 0;         // Minimum number of nodes to consider the network present

    // Peer networking parameters
    bool                        PEER_PRIVATE = false;           // True to ask peers not to relay current IP.
    int                         PEERS_MAX = 0;

    std::chrono::seconds        WEBSOCKET_PING_FREQ = 5min;

    // Path searching
    int                         PATH_SEARCH_OLD = 7;
    int                         PATH_SEARCH = 7;
    int                         PATH_SEARCH_FAST = 2;
    int                         PATH_SEARCH_MAX = 10;

    // Validation
    boost::optional<std::size_t> VALIDATION_QUORUM;     // Minimum validations to consider ledger authoritative

    std::uint64_t                      FEE_DEFAULT = 1000000;
    std::uint64_t                      FEE_ACCOUNT_RESERVE = 10 * SYSTEM_CURRENCY_PARTS;
    std::uint64_t                      FEE_OWNER_RESERVE = 5 * SYSTEM_CURRENCY_PARTS;
    std::uint64_t                      FEE_OFFER = 1000000;

    // Node storage configuration
    std::uint32_t                      LEDGER_HISTORY = 256;
    std::uint32_t                      FETCH_DEPTH = 1000000000;
    int                         NODE_SIZE = 0;

    bool                        SSL_VERIFY = true;
    std::string                 SSL_VERIFY_FILE;
    std::string                 SSL_VERIFY_DIR;

    // Max memo size configuration
    std::uint32_t               MAX_MEMO_SIZE = 1024;
    //int                         MEMO_SIZE = 0;

    // Thread pool configuration
    std::size_t                 WORKERS = 0;

    // Network the server connects to. production = 0, test = 1, development = 2
    // default is production if not specified in the config
    std::uint32_t               PEER_NETWORK = 0;
    bool                        PEER_NETWORK_SET = false;
    
    // These override the command line client settings
    boost::optional<boost::asio::ip::address_v4> rpc_ip;
    boost::optional<std::uint16_t> rpc_port;

    std::unordered_set<uint256, beast::uhash<>> features;

    std::vector<std::string> KYCTrustedAccounts;
    std::vector<std::string> WhitelistAccounts;

public:
    Config() = default;

    int getSize (SizedItemName) const;
    /* Be very careful to make sure these bool params
        are in the right order. */
    void setup (std::string const& strConf, bool bQuiet,
        bool bSilent, bool bStandalone);
    void setupControl (bool bQuiet,
        bool bSilent, bool bStandalone);

    bool reloadFeeVoteParams();
    Json::Value reloadConfigurationVoteParams();
    /**
     *  Load the config from the contents of the string.
     *
     *  @param fileContents String representing the config contents.
     */
    void loadFromString (std::string const& fileContents);

    bool quiet() const { return QUIET; }
    bool silent() const { return SILENT; }
    bool standalone() const { return RUN_STANDALONE; }
};

} // casinocoin

#endif
