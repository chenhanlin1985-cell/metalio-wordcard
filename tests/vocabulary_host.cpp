#include "vocabulary.hpp"
#include <iostream>
using namespace wordcard;
int main(int argc,char**argv){
    try{require(argc==2);VocabularyCatalog c;require(c.load(argv[1]));VocabularyEngine e(c);std::string line;
        while(std::getline(std::cin,line)){try{auto q=VJ::parse(line);auto op=str(q,"op");VJ result;
            if(op=="fsrs"){auto m=vocabulary_fsrs(q["memory"],int(num(q,"rating")),int(num(q,"elapsed")));result=VJ{{"memory",m},{"interval",vocabulary_interval(m)}};}
            else if(op=="create")result=e.create(num(q,"now"),field(q,"legacy"));
            else if(op=="reduce"){VJ events;auto state=e.reduce(q["state"],q["action"],num(q,"now"),events);result=VJ{{"state",state},{"events",events}};}
            else if(op=="task"){auto t=e.current(q["state"]);result=VJ{{"task",t},{"choices",t.is_null()?VJ():e.choices(t,q["state"]["session"])},{"gap",t.is_null()?VJ():e.gap(t)}};}
            else throw std::runtime_error("Unknown host operation");std::cout<<result.dump()<<std::endl;
        }catch(const std::exception&ex){std::cout<<VJ{{"error",ex.what()}}.dump()<<std::endl;}}
        return 0;
    }catch(const std::exception&ex){std::cerr<<ex.what()<<std::endl;return 1;}
}
