#include <ApolloSM/ApolloSM.hh>
#include <ApolloSM/svfplayer.hh>

int ApolloSM::svfplayer(std::string const & svfFile, std::string const & XVCLabel) {

  SVFPlayer SVF;
  int rc = SVF.play(svfFile, XVCLabel);

  return rc;
} 
