#include "quickjs.h"
#include <fstream>
#include <iostream>
#include <string>
int main(int argc,char **argv) {
    if(argc!=2)return 2;
    std::ifstream f(argv[1],std::ios::binary);
    if(!f)return 2;
    std::string test{std::istreambuf_iterator<char>(f),{}};
    auto rt=JS_NewRuntime();auto ctx=JS_NewContext(rt);
    int code=0;
    for(const auto &source:{test}){
        auto value=JS_Eval(ctx,source.data(),source.size(),"pack-test",JS_EVAL_TYPE_GLOBAL);
        if(JS_IsException(value)){
            auto e=JS_GetException(ctx);auto msg=JS_ToCString(ctx,e);
            std::cerr<<(msg?msg:"VM exception")<<std::endl;
            JS_FreeCString(ctx,msg);JS_FreeValue(ctx,e);code=1;
        }
        JS_FreeValue(ctx,value);if(code)break;
    }
    JS_FreeContext(ctx);JS_FreeRuntime(rt);return code;
}
