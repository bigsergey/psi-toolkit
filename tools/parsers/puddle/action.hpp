#ifndef ACTION_H__
#define ACTION_H__

#include "puddle_types.hpp"
#include <string>
#include <vector>
#include <boost/shared_ptr.hpp>
#include "puddle_util.hpp"
#include "lattice_wrapper.hpp"

namespace poleng
{

    namespace bonsai
    {

        namespace puddle
        {

            class Action
            {
                public:
                    Action() { type = "abstract"; }
                    virtual ~Action() { }
                    virtual bool test(Lattice&, int, RuleTokenSizes&,
                            std::list<Lattice::EdgeSequence>&);
                    virtual bool apply(Lattice&, int, RuleTokenSizes&,
                            std::list<Lattice::EdgeSequence>&);
                    virtual std::string getType() { return type; }
                    virtual void setType(std::string aType);

                protected:
                    std::string type;
            };

            typedef boost::shared_ptr<Action> ActionPtr;
            typedef std::vector<ActionPtr> Actions;
            typedef boost::shared_ptr<Actions> ActionsPtr;

        }

    }

}

#endif
