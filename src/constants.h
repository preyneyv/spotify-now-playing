#ifndef __SNP_CONSTANTS_H__
#define __SNP_CONSTANTS_H__

// TODO: in a real project, maybe not great to do this, but this is just a toy
// application sooooooooooooooooooooo
#define SNP_SPOTIFY_AUTH_CLIENT_ID "53dbf71998334c31ab1f6657715621a8"
#define SNP_SPOTIFY_AUTH_PORT 25565
#define SNP_SPOTIFY_AUTH_REDIRECT_URI "http%%3a%%2f%%2flocalhost%%3a25565"

#define SNP_SPOTIFY_AUTH_HOST "https://accounts.spotify.com"
#define SNP_SPOTIFY_AUTH_AUTHORIZE_ENDPOINT SNP_SPOTIFY_AUTH_HOST "/authorize"
#define SNP_SPOTIFY_AUTH_TOKEN_ENDPOINT SNP_SPOTIFY_AUTH_HOST "/api/token"

#define SNP_SPOTIFY_API_HOST "https://api.spotify.com/v1"
#define SNP_SPOTIFY_API_CURRENTLY_PLAYING                                      \
  SNP_SPOTIFY_API_HOST "/me/player/currently-playing"

#endif /* __SNP_CONSTANTS_H__ */
