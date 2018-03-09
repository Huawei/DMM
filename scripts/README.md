To run checkstyle :
./checkstyle.sh  [-options]

To modify license:
./lic.sh

from
    CPUB(lwip_writev)
    NSTACK_CAL_FUN(fdInf->ops, writev, (fdInf->rlfd, iov, iovcnt), size);
	CPUE(lwip_writev)
to	
	CPUB(writev)
    NSTACK_CAL_FUN(fdInf->ops, writev, (fdInf->rlfd, iov, iovcnt), size);
	CPUE(writev)