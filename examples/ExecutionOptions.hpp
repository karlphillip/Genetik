#pragma once
#include <Execution.hpp>
#include <iostream>
#include <initializer_list>
namespace genetik_example {
// Application-only common CLI extraction; leaves established positional/options intact.
inline ExecutionConfig executionOptions(int& argc,char** argv,bool firstPositional=false,bool generationDirectory=false) {
    ExecutionConfig config;bool seen=false;int out=1;
    const auto takesValue=[&](std::string_view option){
        for(auto name:{"--inputs","--target","--seed","--loss-budget","--loss-threshold","--fit-every","--operators","--evaluate","--prime-file","--output"})
            if(option==name)return true;
        return generationDirectory && option=="--generate";
    };
    for(int i=1;i<argc;++i){
        if(firstPositional && i==1){argv[out++]=argv[i];continue;}
        if(std::string_view(argv[i])=="--threads"){
            if(seen || ++i==argc)throw std::invalid_argument("--threads requires one value and may appear once");
            seen=true;config=ExecutionConfig::parse(argv[i]);
        }else{
            const std::string_view option=argv[i];argv[out++]=argv[i];
            // Values (including column/file names literally "--threads") belong
            // to their original option, not to this extraction pass.
            if(takesValue(option) && i+1<argc)argv[out++]=argv[++i];
        }
    }
    argc=out;argv[out]=nullptr;return config;
}
inline void printExecution(ExecutionConfig execution,std::size_t slots) {
    std::cout<<"execution="<<(execution.threads==1?"serial_cpu":"parallel_cpu")
             <<" threads_requested="<<execution.threads
             <<" threads_effective="<<std::min<std::size_t>(execution.threads,slots)
             <<" effective_definition=task_ceiling_for_population caller_evaluates="<<(execution.threads==1)<<'\n';
}
}
