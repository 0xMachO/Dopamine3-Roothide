#!/bin/bash
# The procursus bootstraps bundled with roothide-Dopamine use the *roothide* layout:
#   ./var/                 <- rootfs directly (mobile, lib, cache, ...)
#   ./private/var -> ../var
#   ./var/tmp -> ../tmp
#   ./tmp/                 <- real dir
#   ./.jbroot -> .
#   ./prep_bootstrap.sh, ./usr/libexec/updatelinks.sh, ./.procursus_strapped
#
# They are COMMITTED in this directory:
#   bootstrap_1800.tar.zst   (iOS 15)
#   bootstrap_1900.tar.zst   (iOS 16+, incl. iOS 18)
#
# DO NOT re-download them from apt.procurs.us — that gives the *vanilla* procursus
# layout (rootfs at /var/jb, no /private/var, no /tmp), which makes InstallBootstrap
# ABORT at the /private/var symlink creation step.
#
# To refresh them, copy the current files from the upstream roothide Dopamine repo:
#   Application/Dopamine/Resources/bootstrap_1800.tar.zst
#   Application/Dopamine/Resources/bootstrap_1900.tar.zst

set -e

for f in bootstrap_1800.tar.zst bootstrap_1900.tar.zst; do
  if [ ! -s "$f" ]; then
    echo "ERROR: $f is missing or empty. The roothide bootstrap must be committed." >&2
    exit 1
  fi
done

echo "Using committed roothide bootstraps:"
ls -la bootstrap_1800.tar.zst bootstrap_1900.tar.zst
