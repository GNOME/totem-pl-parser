#!/bin/sh

# Options:
#  -u, --url        URL of the video site page
#  -c, --check      Check whether this URL is supported
#  -d, --debug      Turn on debug mode

# test_videosite
if [ $1 = "--check" ] && [ $2 = "--url" ] && [ $3 = "http://www.youtube.com/watch?v=oMLCrzy9TEs" ]; then
	echo -n "TRUE"
	exit 0
fi

# test_no_url_podcast
if [ $1 = "--check" ] && [ $2 = "--url" ] && [ $3 = "http://www.guardian.co.uk/sport/video/2012/jul/26/london-2012-north-korea-flag-video" ]; then
	echo -n "TRUE"
	exit 0
fi

# test_youtube_starttime
if [ $1 = "--check" ] && [ $2 = "--url" ]; then
	if [ $3 = "http://www.youtube.com/watch?v=Fk2bUvrv-Uc#t=2m30s" ] ||
		[ $3 = "http://www.youtube.com/watch?v=Fk2bUvrv-Uc&t=2m30s" ] ||
		[ $3 = "http://www.youtube.com/embed/Nc9xq-TVyHI?start=110" ]; then
		echo -n "TRUE"
		exit 0
	fi
fi

if [ $1 = "--url" ]; then
	if [ $2 = "http://www.youtube.com/watch?v=Fk2bUvrv-Uc#t=2m30s" ] ||
		[ $2 = "http://www.youtube.com/watch?v=Fk2bUvrv-Uc&t=2m30s" ]; then
		cat << EOF
title=Детали дня 15 мая 2013
id=Fk2bUvrv-Uc
moreinfo=https://www.youtube.com/watch?v=Fk2bUvrv-Uc
url=https://r4---sn-4gxx-hgnz.googlevideo.com/videoplayback?expire=1624480251&ei=m0XTYOy1MIf7xN8P8eGL-A8&ip=78.199.60.242&id=o-AHm5-Kr0vPm3aaUFOdJVR3a4a28rFE6PMp3m-UzA5O8l&itag=18&source=youtube&requiressl=yes&mh=JH&mm=31%2C29&mn=sn-4gxx-hgnz%2Csn-25ge7nzs&ms=au%2Crdu&mv=m&mvi=4&pcm2cms=yes&pl=24&initcwndbps=687500&vprv=1&mime=video%2Fmp4&ns=BS_4OZW99xYPjOn5hUrsTnsF&gir=yes&clen=41441520&ratebypass=yes&dur=593.757&lmt=1368698245994886&mt=1624458288&fvip=4&fexp=24001373%2C24007246&c=WEB&n=GCz7aGVUQRKUef7Y7&sparams=expire%2Cei%2Cip%2Cid%2Citag%2Csource%2Crequiressl%2Cvprv%2Cmime%2Cns%2Cgir%2Cclen%2Cratebypass%2Cdur%2Clmt&sig=AOq0QJ8wRAIgMl2RdtX_oijbGgBoO1LMLTg25BHFaMa3geXUZ3X8TJ4CIFb3WpEyX17LFh4w3HyNkUThezAOO4EvcQ3lFlr8oWM8&lsparams=mh%2Cmm%2Cmn%2Cms%2Cmv%2Cmvi%2Cpcm2cms%2Cpl%2Cinitcwndbps&lsig=AG3C_xAwRAIgTpoU6K-20qh5fpDnARiIdMOoEp1bjoDAD7m6YfNMFjwCICZNusZWy0anXfFRZLYi50iHKuY_94JCiGZarRj3DmSq
image-url=https://i.ytimg.com/vi/Fk2bUvrv-Uc/hqdefault.jpg?sqp=-oaymwEcCNACELwBSFXyq4qpAw4IARUAAIhCGAFwAcABBg==&rs=AOn4CLAtxwMtc56DhhCiuIfaskTWi3An-A
duration=594000.0
starttime=150
EOF
		exit 0
	fi
fi

if [ $1 = "--url" ] && [ $2 = "http://www.youtube.com/embed/Nc9xq-TVyHI?start=110" ]; then
	cat << EOF
title=Dancing Merengue Dog
id=Nc9xq-TVyHI
moreinfo=https://www.youtube.com/watch?v=Nc9xq-TVyHI
url=https://r1---sn-4gxx-hgnz.googlevideo.com/videoplayback?expire=1624480475&ei=e0bTYPCKF9fVxN8PpualuAQ&ip=78.199.60.242&id=o-ANN2xJr5y0bATbNQwj38NxybB9whllIXj1veBPdvQnMS&itag=18&source=youtube&requiressl=yes&mh=G8&mm=31%2C29&mn=sn-4gxx-hgnz%2Csn-25glenez&ms=au%2Crdu&mv=m&mvi=1&pl=24&initcwndbps=687500&vprv=1&mime=video%2Fmp4&ns=mjBmS5MHdz-VZYBb48anQo8F&gir=yes&clen=16852474&ratebypass=yes&dur=188.731&lmt=1428059415664773&mt=1624458529&fvip=1&fexp=24001373%2C24007246&c=WEB&n=Qc6zgOM92Rh10MSvR&sparams=expire%2Cei%2Cip%2Cid%2Citag%2Csource%2Crequiressl%2Cvprv%2Cmime%2Cns%2Cgir%2Cclen%2Cratebypass%2Cdur%2Clmt&lsparams=mh%2Cmm%2Cmn%2Cms%2Cmv%2Cmvi%2Cpl%2Cinitcwndbps&lsig=AG3C_xAwRAIgENlVNj6YAwT_AiEf5P6ryxmmvI0VYCFWDsZl5Eb7_zsCIGQtqzxmcf0tt9JI69hnwVvDdiZ_DhcBG5mZcU4vreEB&sig=AOq0QJ8wRgIhAKldrkc_5cidkcIdrsrSnHtS5_XPhYOIG1FYi39RO2aYAiEAq7SZbtK9yjK-8dOp90Nc0UwOjQnRcKiuPck9q16UzRw=
image-url=https://i.ytimg.com/vi/Nc9xq-TVyHI/hqdefault.jpg?sqp=-oaymwEcCNACELwBSFXyq4qpAw4IARUAAIhCGAFwAcABBg==&rs=AOn4CLDpYfSmD8FyLDFNdw7eOKuhzuiFuA
duration=189000.0
starttime=110
EOF
	exit 0
fi

# test_parsing_rss_link
if [ $1 = "--check" ] && [ $2 = "--url" ] && [ $3 = "http://www.guardian.co.uk/technology/audio/2011/may/03/tech-weekly-art-love-bin-laden" ]; then
	echo -n "TRUE"
	exit 0
fi

# test_video_links_slow_parsing
if [ x$SLOW_PARSING != x ] ; then
	sleep 1
fi

echo -n "FALSE"
