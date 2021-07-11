# fuse-fs
A user-space filesystem based on [FUSE](https://github.com/libfuse/libfuse)。

Features supported:

- Multi-level directory and file
- Block cache manager
- Permission control given user and group id
- Single-password encryption to the whole disk
- 17 fuse interfaces supported: getattr, opendir, readdir, releasedir, open, read, write, fsync, release, create, mkdir, unlink, rmdir, rename, link, chmod, chown

### Reference

- [Fusepp](https://github.com/jachappell/Fusepp)

- [easy-fs](https://github.com/rcore-os/rCore-Tutorial-v3/tree/main/easy-fs/src) for rCore

### 课程总结

期中写了B+树，期末大家都照着OS的文件系统抄，只是抄的早晚问题。我只报名了期中展示，有两组期中期末都展示了，期末也确实是一个板子出来的。我期末迟交了一周，只拿了个A-，可以说亏炸了，至于为什么迟交？这就不得不提助教改ddl的骚操作了。

发现错过ddl时已经晚了，更要命的是接着就是考试周（虽然我只有系统结构一门考试^_^），我只能挤出预习时间赶紧补，又多写了个权限和加密以求尽量挽回。事实证明这些操作包括展示加分都没用。

我在一开始想着能不能写点新奇的玩意出来，以为18周截止有充足的时间。事后看来，直接花两三天把OS的文件系统抄过来完事，再报名个期末展示，毕竟不明确的大作业给分要求+紧迫的时间，没有谁能体现出远超过其他人的工作量/能力。

我期中写了一千多行代码，期末也有两三千行了，再对比下七字班某神，期中期末加起来也就一千行拿了A+，真是高下立判。
