---
layout: post
title: Getting DMESG_RESTRICT Enabled in Ubuntu 20.10 Groovy Gorilla
categories: Programming Writeups
---

You might have noticed a small change when running the `dmesg` command in
Ubuntu 20.10 Groovy Gorilla, since it now errors out with:

`dmesg: read kernel buffer failed: Operation not permitted`

Don't worry, it still works, it has just become a privileged operation, and it
works fine with `sudo dmesg`. But why the change?

Well, I happen to be the one who proposed for this change to be made, and
followed up on getting the configuration changes made. This blog post will
describe how it slightly improves the security of Ubuntu, and the journey to
getting the changes landed in a release.

![hero](/assets/images/2020_020.png)

So stay tuned, and let's dive into `dmesg`.

<!--more-->

# What is dmesg?

`dmesg` is a command that allows you to view the kernel log buffer. The kernel
log buffer contains a whole wealth of information about system hardware, devices
attached and their allocated memory regions, and error logging for the system.

This log buffer usually lives at `/dev/kmsg` or `/proc/kmsg`, which is what
tools like `dmesg` or `journalctl` or various `syslog` programs read from.

If we look at some typical start-up information, it really isn't too interesting.

![early dmesg](/assets/images/2020_021.png)

# Why is restricting dmesg important?

The thing is, the kernel log buffer can sometimes contain all sorts of security
critical information, such as pointers to kernel memory. There has been a large
effort in the mainline kernel for a few years now to remove all instances of
`printk("%p")`, which leaked raw kernel pointers to the kernel log buffer.

These days, all `%p` format strings hash the kernel pointer, so the address
itself is not leaked, but still gives a unique identifier for developers to
look at when doing `printk`.

However, kernel pointers can still be leaked in other ways, such as if the system
suffers an oops, it will print the current kernel stacktrace, as well as provide
a copy of register values:

```
 [3191370.893495] WARNING: CPU: 13 PID: 48929 at /build/linux-hwe-FEhT7y/linux-hwe-4.15.0/include/linux/mm.h:852 follow_page_pte+0x6f4/0x710
 [3191370.893552] CPU: 13 PID: 48929 Comm: CPU 0/KVM Not tainted 4.15.0-106-generic #107~16.04.1-Ubuntu
 [3191370.893552] Hardware name: Dell Inc. PowerEdge R740xd/00WGD1, BIOS 2.6.4 04/09/2020
 [3191370.893554] RIP: 0010:follow_page_pte+0x6f4/0x710
 [3191370.893555] RSP: 0018:ffffad279f7ab908 EFLAGS: 00010286
 [3191370.893556] RAX: ffffdc0fa72eba80 RBX: ffffdc0f9b1535b0 RCX: 0000000080000000
 [3191370.893556] RDX: 0000000000000000 RSI: 00003ffffffff000 RDI: 800000b9cbaea225
 [3191370.893557] RBP: ffffad279f7ab970 R08: 800000b9cbaea225 R09: ffff9359857fd5f0
 [3191370.893558] R10: 0000000000000000 R11: 0000000000000000 R12: ffffdc0fa72eba80
 [3191370.893558] R13: 0000000000000326 R14: ffff935de09e19e0 R15: ffff9359857fd5f0
 [3191370.893559] FS:  00007f68757fa700(0000) GS:ffff93617ef80000(0000) knlGS:ffff964a7fc00000
 [3191370.893559] CS:  0010 DS: 0000 ES: 0000 CR0: 0000000080050033
 [3191370.893560] CR2: 00007ff92ca7a000 CR3: 000000b7209d2005 CR4: 00000000007626e0
 [3191370.893561] DR0: 0000000000000000 DR1: 0000000000000000 DR2: 0000000000000000
 [3191370.893561] DR3: 0000000000000000 DR6: 00000000fffe0ff0 DR7: 0000000000000400
 [3191370.893561] PKRU: 55555554
 [3191370.893562] Call Trace:
 [3191370.893565]  follow_pmd_mask+0x273/0x630
 [3191370.893567]  ? gup_pgd_range+0x23f/0xde0
 [3191370.893568]  follow_page_mask+0x178/0x230
 [3191370.893569]  __get_user_pages+0xb8/0x740
 [3191370.893571]  get_user_pages+0x42/0x50
 [3191370.893604]  __gfn_to_pfn_memslot+0x18b/0x3b0 [kvm]
 [3191370.893615]  ? mmu_set_spte+0x1dd/0x3a0 [kvm]
 [3191370.893626]  try_async_pf+0x66/0x220 [kvm]
 [3191370.893635]  tdp_page_fault+0x14b/0x2b0 [kvm]
 [3191370.893640]  ? vmexit_fill_RSB+0x10/0x40 [kvm_intel]
 [3191370.893649]  kvm_mmu_page_fault+0x62/0x180 [kvm]
 [3191370.893651]  handle_ept_violation+0xbc/0x160 [kvm_intel]
 [3191370.893654]  vmx_handle_exit+0xa5/0x580 [kvm_intel]
 [3191370.893664]  vcpu_enter_guest+0x414/0x1260 [kvm]
 [3191370.893674]  kvm_arch_vcpu_ioctl_run+0xd9/0x3d0 [kvm]
 [3191370.893683]  ? kvm_arch_vcpu_ioctl_run+0xd9/0x3d0 [kvm]
 [3191370.893691]  kvm_vcpu_ioctl+0x33a/0x610 [kvm]
 [3191370.893693]  ? audit_filter_rules+0x232/0x1070
 [3191370.893696]  do_vfs_ioctl+0xa4/0x600
 [3191370.893697]  ? __audit_syscall_entry+0xac/0x100
 [3191370.893699]  ? syscall_trace_enter+0x1d6/0x2f0
 [3191370.893700]  SyS_ioctl+0x79/0x90
 [3191370.893701]  do_syscall_64+0x73/0x130
 [3191370.893704]  entry_SYSCALL_64_after_hwframe+0x3d/0xa2
```

If kernel pointers happen to be in the registers at the time of oops, they get
leaked to the kernel log buffer.

Kernel pointers are valuable to attackers and exploit developers, because they
act as *information leaks*. These information leaks make it much easier to
de-randomise the kernel base address and to defeat KASLR. If an attacker is
trying to launch a privilege escalation attack against a recently compromised
host, they can also use dmesg to get instant feedback on their exploits, as
failures will cause further oops messages or segmentation faults. This makes it
easier for attackers to fix and tune their exploit programs until they work.

Currently, if I create a new, unprivileged user on a Focal system, they cannot
access `/var/log/kern.log`, `/var/log/syslog` or see system events in `journalctl`.
But yet, they are given free reign to the kernel log buffer.

```
$ sudo adduser dave
$ su dave
$ groups
dave
$ cat /var/log/kern.log
cat: /var/log/kern.log: Permission denied
$ cat /var/log/syslog
cat: /var/log/syslog: Permission denied
$ journalctl
Hint: You are currently not seeing messages from other users and the system.
      Users in groups 'adm', 'systemd-journal' can see all messages.
      Pass -q to turn off this notice.
Jun 16 23:44:59 ubuntu systemd[2328]: Reached target Main User Target.
Jun 16 23:44:59 ubuntu systemd[2328]: Startup finished in 69ms.
$ dmesg
[    0.000000] Linux version 5.4.0-34-generic (buildd at lcy01-amd64-014)
(gcc version 9.3.0 (Ubuntu 9.3.0-10ubuntu2)) #38-Ubuntu SMP Mon May 25 15:46:55
UTC 2020 (Ubuntu 5.4.0-34.38-generic 5.4.41)
[    0.000000] Command line: BOOT_IMAGE=/boot/vmlinuz-5.4.0-34-generic
root=UUID=f9f909c3-782a-43c2-a59d-c789656b4188 ro
```

Strange how an unprivileged user can read dmesg just fine, and yet cannot access
any other kernel logs on the system.

# The Initial Proposal

I sent a proposal to `ubuntu-devel` in June which outlines the above problems,
to gather some feedback and to see if anyone else thinks that this is a good
idea.

[Proposal: Enabling DMESG_RESTRICT for Groovy Onward](https://lists.ubuntu.com/archives/ubuntu-devel/2020-June/041063.html)

![proposal](/assets/images/2020_022.png)

I suggested that we restrict access to dmesg to users in group 'adm' like so:

1. `CONFIG_SECURITY_DMESG_RESTRICT=y` in the kernel.
2. Following changes to `/bin/dmesg` permissions in package `util-linux`
    - Ownership changes to `root:adm`
    - Permissions changed to `0750 (-rwxr-x---)`
    - Add `cap_syslog` capability to binary.
3. Add a commented out `# kernel.dmesg_restrict = 0` to
   `/etc/sysctl.d/10-kernel-hardening.conf`

Let's break these down.

Number 1 is how `DMESG_RESTRICT` gets enforced, as setting `CONFIG_SECURITY_DMESG_RESTRICT=y`
in the kernel config restricts the kernel log buffer to executables with
`CAP_SYSLOG`, or root privileges. 

Number 2 allows users in the `adm` group, also known as "administration", to
be able to execute dmesg without becoming super user, which means nothing
would change for default users in most systems.

Number 3 adds a easy way for system administrators to disable the change if they
want.

I filed a Launchpad bug to document the changes and track the patches I had
created for `util-linux` and `procps`.

[LP #1886112 Enabling DMESG_RESTRICT in Groovy Onward](https://bugs.launchpad.net/bugs/1886112)

## Early Responses and Getting the Kernel Config Changed (1)

The security team were +1 with the change:

[https://lists.ubuntu.com/archives/ubuntu-devel/2020-June/041067.html](https://lists.ubuntu.com/archives/ubuntu-devel/2020-June/041067.html)

When I woke up the next day, the strangest thing happened. [Phoronix](https://www.phoronix.com)
had written an article about my proposal!

[Ubuntu 20.10 Looking At Restricting Access To Kernel Logs With dmesg](https://www.phoronix.com/scan.php?page=news_item&px=Ubuntu-20.10-Restrict-dmesg)

This wasn't expected at all, and it got people talking about the change in
forums, instead of it just being silently made and me hoping that no one noticed.

After that, Seth Forshee, from the kernel team, double checked with the security
team, and then went ahead and applied the change to the "unstable" kernel tree, 
since Groovy's kernel had not yet forked off from it at that point in time.

[https://lists.ubuntu.com/archives/ubuntu-devel/2020-July/041079.html](https://lists.ubuntu.com/archives/ubuntu-devel/2020-July/041079.html)

The kernel commit is:

```
Commit 25e6c851704a47c81e78e1a82530ac4b328098a6
From: Seth Forshee <seth.forshee@canonical.com>
Date: Thu, 2 Jul 2020 13:29:55 -0500
Subject: UBUNTU: [Config] CONFIG_SECURITY_DMESG_RESTRICT=y
Link: https://kernel.ubuntu.com/git/ubuntu/unstable.git/commit/?id=25e6c851704a47c81e78e1a82530ac4b328098a6
```

Now that the configuration change was made in the kernel, Number 1 in the
list was completed.

## Upstream Discussions for Adding CAP_SYSLOG to /bin/dmesg (2)

At this point, things got a bit stuck. I got busy and no one else replied to my
previous posts, so the changes to `util-linux` got a little delayed.

I restarted these talks with the below message to `ubuntu-devel`, and included
the upstream Debian maintainers to the CC list.

[https://lists.ubuntu.com/archives/ubuntu-devel/2020-August/041117.html](https://lists.ubuntu.com/archives/ubuntu-devel/2020-August/041117.html)

This was successful, and Chris Hofstaedtler, wrote back. Chris asked if this had
been discussed before in Debian:

[https://lists.ubuntu.com/archives/ubuntu-devel/2020-August/041118.html](https://lists.ubuntu.com/archives/ubuntu-devel/2020-August/041118.html)

I responded with what I could find, but I also mentioned that I would write
to `debian-devel`.

[https://lists.ubuntu.com/archives/ubuntu-devel/2020-August/041125.html](https://lists.ubuntu.com/archives/ubuntu-devel/2020-August/041125.html)

So, I went and proposed similar changes to `debian-devel` in this thread:

[https://lists.debian.org/debian-devel/2020/08/msg00107.html](https://lists.debian.org/debian-devel/2020/08/msg00107.html)

I got some positive responses, but the most interesting one was from Ansgar:

[https://lists.debian.org/debian-devel/2020/08/msg00121.html](https://lists.debian.org/debian-devel/2020/08/msg00121.html)

Ansgar mentioned that if `/bin/dmesg` is granted `CAP_SYSLOG`, and `/bin/dmesg`
was opened up to users of group `adm`, then any user of `adm` could clear the
kernel log buffer by running `$ dmesg --clear`.

Now, I had missed this, and it was an excellent catch.

We don't want to make it easier for anyone to clear the kernel log buffer, since
it can be used to hide an attackers presence, so adding `CAP_SYSLOG` to `/bin/dmesg`
is a bad idea.

Chris mentions this in his message back:

[https://lists.ubuntu.com/archives/ubuntu-devel/2020-August/041151.html](https://lists.ubuntu.com/archives/ubuntu-devel/2020-August/041151.html)

From there, Steve Langasek also mentioned that it was a bad idea:

[https://lists.ubuntu.com/archives/ubuntu-devel/2020-August/041152.html](https://lists.ubuntu.com/archives/ubuntu-devel/2020-August/041152.html)

and with that, I decided to drop the idea of adding `CAP_SYSLOG` to `/bin/dmesg`
and changing the group to `adm`:

[https://lists.ubuntu.com/archives/ubuntu-devel/2020-August/041153.html](https://lists.ubuntu.com/archives/ubuntu-devel/2020-August/041153.html)

That makes Number 2 on the list struck off. It's a bit of a pity, since it means
that users in group `adm` have to write `$ sudo dmesg` instead of `$ dmesg`.
Hopefully it won't be too much of a bother to become superuser to view dmesg.
Time will tell I suppose, and most distros follow this behaviour anyway.

## Landing sysctl Configuration Changes (3)

Shortly after the upstream `util-linux` discussion ended, Brian Murrary sponsored
my patches to `procps` to add some documentation about `CONFIG_SECURITY_DMESG_RESTRICT`
and instructions on how to disable it by changing a sysctl variable.

![sysctl](/assets/images/2020_023.png)

As my description states, if you want to turn off `DMESG_RESTRICT`, you can
do so by uncommenting the sysctl string `kernel.dmesg_restrict = 0`, and
rebooting.

With this, Number 3 in the list was completed.

# Conclusion

That is the story of how `DMESG_RESTRICT` was enabled in Ubuntu 20.10 Groovy
Gorilla. We covered how it slightly improves system security by removing an avenue
attackers could use to view leaked kernel pointers, the process of getting all
the separate changes landed, and relevant upstream discussions.

I hope you enjoyed the read, and if you have any questions or comments, feel 
free to [contact me](/about).

Matthew Ruffell
