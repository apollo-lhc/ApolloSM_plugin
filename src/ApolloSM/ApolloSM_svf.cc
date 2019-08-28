#include <ApolloSM/ApolloSM.hh>
#include <ApolloSM/svfplayer.hh>

int ApolloSM::svfplayer(std::string const & svfFile, std::string const & XVCReg) {

  SVFPlayer SVF(GetHWInterface());
  int rc = SVF.play(svfFile, XVCReg);

  if(rc == 0) {fprintf(stderr, "SVFplayer ran without errors.\n");}
  else {fprintf(stderr, "SVFplayer ran with errors.\n");}

  return rc;
} 
