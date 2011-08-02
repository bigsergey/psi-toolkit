#ifndef ACTION_H__
#define ACTION_H__

//#include <boost/regex.hpp>
//#include <boost/regex/icu.hpp>

//#include "utf8_converter.hpp"

#include "puddle_types.hpp"
#include "entity.hpp"
#include "TransitionInfo.hpp"
//#include "TransitionGraph.hpp"
#include <string>
#include <vector>
#include <boost/shared_ptr.hpp>

namespace poleng
{

namespace bonsai
{

namespace puddle
{

    //typedef boost::u32regex Pattern;

class Action
{
    public:
        Action() { type = "abstract"; verbose = false; }
        virtual bool test(Entities entities, int currentEntity, std::vector<int> matchedTokensSize); //tu nie bedzie to na napisie wykonywane, tylko na tych obiektach?
        virtual bool apply(Entities &entities, Edges &edges, int currentEntity, std::vector<int> matchedTokensSize); //jw
        virtual std::string getType() { return type; }
        void setType(std::string aType);

        virtual void setVerbose() { verbose = true; }

    private:
        std::string type;
        bool verbose;

        //std::vector<std::string> arguments;

};

typedef boost::shared_ptr<Action> ActionPtr;
typedef std::vector<ActionPtr> Actions;
typedef boost::shared_ptr<Actions> ActionsPtr;

}

}

}

#endif
