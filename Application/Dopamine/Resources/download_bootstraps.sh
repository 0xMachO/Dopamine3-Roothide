set -e

curl -L https://apt.procurs.us/bootstraps/1800/bootstrap-iphoneos-arm64.tar.zst --output bootstrap_1800.tar.zst
curl -L https://apt.procurs.us/bootstraps/1900/bootstrap-iphoneos-arm64.tar.zst --output bootstrap_1900.tar.zst

# Package managers are the roothide forks (arm64e, link @loader_path/.jbroot/usr/lib/libroothide.dylib).
# Committed in Resources/ (sileo.deb / zebra.deb). To refresh from roothide.github.io/procursus:
#   curl -L https://raw.githubusercontent.com/roothide/roothide.github.io/main/debfiles/org.coolstar.sileo_2.5.1-13_iphoneos-arm64e.deb --output sileo.deb
#   curl -L https://raw.githubusercontent.com/roothide/roothide.github.io/main/debfiles/xyz.willy.zebra_1.1.36-2-1+debug_iphoneos-arm64e.deb --output zebra.deb
