#include <cosmos/cosmos.hpp>
#include <data/encoding/ascii.hpp>
#include <abstractions/script/pow.hpp>
#include <iostream>

namespace cosmos {
    
    namespace pow {
            
        class error : std::exception {
            std::string Message;
                
        public:
            error(std::string s) : Message{s} {}
            
            const char* what() const noexcept final override {
                return Message.c_str();
            }
        };
    
        bitcoin::address read_address_from_script(const bytes&);
        uint read_uint(const std::string&);
        bitcoin::secret read_wif(const std::string&);
        data::encoding::ascii::string read_ascii(const std::string&);
        byte read_byte(const std::string&);
        abstractions::work::uint24 read_uint24(const std::string&);
        
        bitcoin::work::candidate read_candidate(
            const std::string& message, 
            const std::string& exponent,
            const std::string& value) {
            bytes b = bytes(read_ascii(message));
            if (b.size() != abstractions::work::message_size) throw error{"wrong message size. Must be 68 characters."};
            abstractions::work::message m{};
            std::copy(b.begin(), b.end(), m.begin());
            return bitcoin::work::candidate{m,
                bitcoin::work::target{read_byte(exponent), read_uint24(value)}};
        }
        
        inline bitcoin::script lock(bitcoin::work::candidate c) {
            return *abstractions::script::lock_by_pow(c);
        }
        
        bitcoin::transaction main(
            bitcoin::transaction prev,
            bitcoin::outpoint ref, 
            bitcoin::secret key,
            bitcoin::work::candidate candidate)
        {
            if (ref.Reference != prev.id()) throw error{"invalid reference to previous tx"};
            bitcoin::transaction::representation previous{prev};
            if (!previous.Valid) throw error{"invalid tx"};
            if (previous.Outputs.size() >= ref.Index) throw error{"No such output in previous tx"};
                
            bitcoin::output redeemed{previous.Outputs[ref.Index]};
            
            bitcoin::address address = read_address_from_script(redeemed.ScriptPubKey);
            if (!address.valid()) throw error{"invalid output script"};
            if (address != bitcoin::address{key}) throw error{"cannot redeem address with key"};
            
            bitcoin::vertex vertex{bitcoin::vertex{{bitcoin::vertex::spendable{key, redeemed, ref}}, {bitcoin::output{redeemed.Value, lock(candidate)}}}};
            
            return bitcoin::redeem({}, vertex);
        }

        struct program {
            bitcoin::transaction Previous;
            bitcoin::outpoint Reference;
            bitcoin::secret Key;
            bitcoin::work::candidate Candidate;
            
            program(
                bitcoin::transaction t, 
                bitcoin::outpoint r, 
                bitcoin::secret k, 
                bitcoin::work::candidate c) : Previous{t}, Reference{r}, Key{k}, Candidate{c} {}
        
            static program make(list<std::string> input) {
                if (input.size() != 6) throw error{"six inputs required"};
                bitcoin::transaction p{input[0]};
                if (!p.valid()) throw error{"transaction is not valid"};
                return program{p, bitcoin::outpoint{p.id(), read_uint(input[1])}, read_wif(input[2]), read_candidate(input[3], input[4], input[5])};
            }
            
            bool valid() const {
                return Previous.valid() && Key.valid();
            }
            
            bitcoin::transaction operator()() {
                return pow::main(Previous, Reference, Key, Candidate);
            };
        };
    
    }

    list<std::string> read_input(int argc, char* argv[]);
    
}
        
int main(int argc, char* argv[]) {
    using namespace cosmos;
    using namespace std;
    
    std::string out;
    
    try {
        out = data::encoding::hex::write(pow::program::make(read_input(argc, argv))());
    } catch (std::exception& e) {
        out = e.what();
    }
    
    cout << out;
    
    return 0;
}
