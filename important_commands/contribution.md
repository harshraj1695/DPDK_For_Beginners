# CONTRIBUTION.md

## Contributing a New Example to DPDK (Step-by-Step)

This guide explains how to contribute a new example to DPDK, starting
from cloning the repository up to sending a patch to the mailing list.

It also includes common issues encountered during the process and how to
fix them.

------------------------------------------------------------------------

## 1. Fork and Clone DPDK

Fork the official repository on GitHub:

https://github.com/DPDK/dpdk

Clone your fork:

``` bash
git clone https://github.com/<your-username>/dpdk.git
cd dpdk
```

Add upstream remote:

``` bash
git remote add upstream https://github.com/DPDK/dpdk.git
git fetch upstream
```

Verify remotes:

``` bash
git remote -v
```

You should see both **origin** and **upstream**.

------------------------------------------------------------------------

## 2. Create a Feature Branch

Always branch from upstream `main`:

``` bash
git checkout -b l2fwd-acl-example upstream/main
```

Do **not** branch from your fork's `main` branch.

------------------------------------------------------------------------

## 3. Add Your Example Code

Create directory:

    examples/l2fwd_acl/

Add files:

    examples/l2fwd_acl/main.c
    examples/l2fwd_acl/meson.build
    examples/l2fwd_acl/README.md

------------------------------------------------------------------------

## 4. Register the Example in Meson

Edit:

    examples/meson.build

Add:

    'l2fwd_acl',

If this step is skipped, the example will **not build**.

------------------------------------------------------------------------

## 5. Build DPDK With Examples Enabled

Examples are disabled by default.

``` bash
meson setup build -Dexamples=l2fwd_acl
ninja -C build
```

Check:

``` bash
ls build/examples/
```

You should see:

    dpdk-l2fwd_acl

------------------------------------------------------------------------

## 6. Common Build Errors and Fixes

### Example not appearing in `build/examples`

**Cause:** Example not enabled or not registered.

**Fix:**

``` bash
meson setup build -Dexamples=l2fwd_acl --reconfigure
ninja -C build
```

------------------------------------------------------------------------

### Meson error: `unacceptable type 'str'`

**Cause:** Dependencies written as strings.

**Wrong:**

``` meson
deps += ['acl']
```

**Correct:**

``` meson
deps += [dep_acl, dep_ethdev, dep_mbuf, dep_net]
```

------------------------------------------------------------------------

### Logging error `RTE_LOGTYPE_LOGTYPE undeclared`

**Cause:** Incorrect log type macro.

**Fix:**

``` c
#define RTE_LOGTYPE_L2FWD_ACL RTE_LOGTYPE_USER1
```

Use:

``` c
RTE_LOG(INFO, L2FWD_ACL, "message");
```

------------------------------------------------------------------------

## 7. Commit Your Changes

``` bash
git add examples/l2fwd_acl
git add examples/meson.build
git commit -s
```

------------------------------------------------------------------------

## 8. Generate Patch

``` bash
git format-patch upstream/main
```

You should get:

    0001-examples-l2fwd_acl-....patch

------------------------------------------------------------------------

## 9. Configure git send-email

``` bash
git config --global sendemail.smtpserver smtp.gmail.com
git config --global sendemail.smtpserverport 587
git config --global sendemail.smtpencryption tls
git config --global sendemail.smtpuser yourmail@gmail.com
git config --global sendemail.from "Your Name <yourmail@gmail.com>"
```

Use a Gmail App Password:

``` bash
git config --global sendemail.smtppass YOUR_APP_PASSWORD
```

------------------------------------------------------------------------

## 10. Send Patch

``` bash
git send-email 0001-*.patch --to dev@dpdk.org
```

------------------------------------------------------------------------

## 11. Possible Email Issues

### SMTP initialization failure

Run with debug:

``` bash
git send-email --smtp-debug=1 0001-*.patch --to dev@dpdk.org
```

Ensure: - App password used - TLS enabled - Port 587 - No VPN blocking
SMTP

------------------------------------------------------------------------

### Patch held by moderator

This is normal for first-time contributors.\
Wait for approval or subscribe to the mailing list.

------------------------------------------------------------------------

## 12. Done

You have successfully contributed a new example to DPDK.
