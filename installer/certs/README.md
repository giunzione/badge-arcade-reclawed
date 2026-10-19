# CA certificates

`cacert.pem` is Mozilla's list of trusted root certificates, in the PEM format
published by the curl project. The app bundles it in its RomFS so libcurl can
verify GitHub's HTTPS certificates when it downloads the latest patches.

- Source: https://curl.se/ca/cacert.pem ("Certificate data from Mozilla as of
  Fri Sep 25 03:12:01 2026 GMT"), 121 certificates
- sha256 `a41b5d356aea97a529fe27e0f7316d2f9d946d75927476cf9cf1b90637d00505`,
  matching https://curl.se/ca/cacert.pem.sha256 at download time
- License: MPL 2.0 (the certificate data comes from Mozilla's NSS)

To update it, download both files again, check the hash, and replace `cacert.pem`.
GitHub's servers currently chain to *USERTrust ECC Certification Authority*
(github.com) and *ISRG Root X1* (the release download servers).
