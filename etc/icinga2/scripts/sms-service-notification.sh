#!/usr/bin/env bash
#
# send service notifications via short message (SMS)
# 
# uses smstools http://smstools3.kekekasvi.com/
# the icinga command user must be member in the group smstools uses

SMSTOOLS_GROUP=smstools
SMSTOOLS_OUTGOING_DIR=/var/spool/sms/outgoing

TMPFILE=`mktemp /tmp/smsd_XXXXXX`

echo "To: $USERPAGER" >$TMPFILE
echo "Alphabet: ISO" >>$TMPFILE
echo "" >>$TMPFILE

DURATION_MIN=$[$DURATION_SEC/60]

echo "$NOTIFICATIONTYPE - $HOSTDISPLAYNAME - $SERVICEDISPLAYNAME is $SERVICESTATE for ${DURATION_MIN}m" | \
	iconv -t ISO-8859-15 >>$TMPFILE

if [ -n "$NOTIFICATIONCOMMENT" ]; then
	echo "[$NOTIFICATIONAUTHORNAME] ${NOTIFICATIONCOMMENT:0:100}" | \
		iconv -t ISO-8859-15 >>$TMPFILE
fi

echo "(${SERVICEOUTPUT:0:100})" | iconv -t ISO-8859-15 >>$TMPFILE

chgrp $SMSTOOLS_GROUP $TMPFILE
chmod 660 $TMPFILE

RETRY=10
while ! mv -n $TMPFILE $SMSTOOLS_OUTGOING_DIR; do
	# target file seems to already exist

	if [ $RETRY -le 0 ]; then
		echo "can't write to target directory"
		exit 1
	fi

	NEWTMP=`mktemp /tmp/smsd_XXXXXX`
	mv -f $TMPFILE $NEWTMP
	TMPFILE=$NEWTMP

	RETRY=$[$RETRY-1]
done

