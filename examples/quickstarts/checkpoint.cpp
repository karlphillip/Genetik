#include <ExpressionFormatting.hpp>
#include <ScalarExpressionCheckpoint.hpp>
#include <array>
#include <iostream>
int main(){
    const auto problem=RegressionProblem::create({{{2},3}},{3,2,8});
    const auto expression=Expression::binary(ExpressionKind::Add,Expression::variable(0,1,{3,2,8}),Expression::constant(1,1,{3,2,8}),{3,2,8});
    RandomCheckpoint randomState;std::string bytes;
    {
        RandomContext random(42);
        Population<ExpressionCandidate,RegressionOrdering> population(std::vector<ExpressionCandidate>{ExpressionCandidate(expression,problem)},random,ObjectiveDirection::Minimize);
        CheckpointWriter writer;writeCommittedPopulation(writer,RunCheckpointAccess::population(population));
        CheckpointDocument d;d.codec=CandidateCheckpointCodec<ExpressionCandidate>::identifier;d.identity="quickstart:v1:x0=2,target=3,limits3/2/8";d.payload=writer.data();d.generation=0;
        bytes=encodeCheckpoint(d);randomState=RunCheckpointAccess::random(random);
    } // original population and run context destroyed
    const auto d=decodeCheckpoint(bytes);
    if(d.codec!=CandidateCheckpointCodec<ExpressionCandidate>::identifier||d.identity!="quickstart:v1:x0=2,target=3,limits3/2/8")return 1;
    CheckpointReader reader(d.payload);const auto state=readCommittedPopulation<ExpressionCandidate>(reader,ScalarCheckpointContext{problem,{}});reader.finish();
    RandomContext resumed(42);RunCheckpointAccess::restoreRandom(resumed,randomState);
    auto restored=RunCheckpointAccess::restorePopulation<ExpressionCandidate,RegressionOrdering>(state,resumed);
    if(restored->generation()!=0||restored->bestSeen()->fitness()!=0||resumed.evaluationAttempts()!=1)return 1;
    std::cout<<formatExpression(restored->bestSeen()->expression())<<'\n'
             <<formatExpression(restored->bestSeen()->expression(),ExpressionTextFormat::LaTeX)<<"\nowned committed population restored without evaluation\n";
}
