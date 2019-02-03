##################################
Nw Testing/Data transfer logic
##################################
:version: v20190204IST0244-NeedsUpdating
:author: HanishKVC, 19XY

.. contents::
.. section-numbering::

.. raw:: pdf

   PageBreak



Logic and Usage
#################


Overview
==========

This set of programs helps verify network performance as well as if required
transfer a file from server to multiple clients.


The Phases
------------

It consits of

Multicast based transfer logic

Multicast based stop logic

Unicast based Presence Info logic - to help clients and server come to know
about one another. Logic can work even if no communication during PI phase,
provided a known list of clients is provided before hand itself to the server.

Unicast based data / lost packet recovery logic - The server communicates with
the clients one by one and gets their list of lost packets, a small part at a
time, and helps them recover those by resending it thro unicast.


MulticastTransfer
~~~~~~~~~~~~~~~~~~

In this phase either auto generated test blocks or contents of a specified file
are blindly sent (i.e without checking who all are actively listening and
neither worrying about when clients join into this multicast channel and when
they leave) over the specified multicast channel at the specified byterate.

And at the end a stop command is sent on the multicast channel to inform the
clients that the multicast transfer is over.


PI Phase
~~~~~~~~~~

If the PI phase fails to handshake between the server and the clients, the
server has the possibility of using a predefined list of clients to work with,
which is given to it, thro its context option.  While the client eitherway will
now respond to any server which requests it for list of lost packets.

A PI handshake consists of Server sending the PIReq (mcasted) and inturn
waiting for ucast PIAcks from the clients. The server waits for a predefined
time to give all clients enough time to respond. However if some known clients
have still not responded, then server will try PI handshake again, till a given
maximum number of attempts. Also if the server is not preovided with a known
client list, then the server will keep trying PI handshake for the specified
number of times, irrespective of any client responded to a given handshake or
not.

The MCast Transfer involves a medium duration PI phase at the beginning. It
uses this to build a list of known clients, which inturn it uses for the
periodic minimal PI phases which it triggers spread across the mcast transfer
based on the predefined interval. The periodic minimal PI queries help server
get a feel of how the clients are performing from transfer perspective, as well
as the server uses this to request the clients to save their context during
this time. This ensures that even if there is a unexpected failure on the
client side, the transfer till that point is not fully lost. A context save is
even requested almost immidiately after the mcast transfer as started.

Wrt UCast recovery, a PI phase is triggered at the beginning, before the actual
UCast recovery is attempted.

Client informs the Server about its name/clientId and the total lost packets it
has to recover as part of the PIReq packet it sends.


UnicastRecovery
~~~~~~~~~~~~~~~~~
If a client stops responding in the middle of unicast error recovery or has
used up too many attempts and has still not fully recovered its lost packets,
then the server side logic will gracefully keep that client aside, and go to
the remaining clients. In turn at the end it will come back and check which
clients had been kept aside and then will try to help those clients recover
their lost packets.

Depending on the length of the content transfered, the server logic will decide
for how many times it should run thro the kept aside clients lists. Even after
that if there are clients which haven't fully recovered, the server will list
those clients and give up.

The logic will assume that upto a max of 8% to 10% packet losess could be there
and based on that decides how many attempts it should try wrt clients that keep
getting kept aside.

NOTE: If a client doesn't respond back to the server for upto N(1.5) minutes or
if it has not recovered all its lost packets even after handshaking with it for
512 times, then it is kept aside temporarily.

When the client communicates with server with URAckSeqNum, it not only gives a
small list of lost packet ranges to recover immidiately, but also in total how
many lost packet ranges (i.e iNodeCnt - the number of nodes in the list) are
there as well as inturn the total number of lost packets that are there to be
recovered (i.e iTotalFromRanges) at that given time in the client. These counts
also include the packets specified in the URAckSeqNum packet for immidiate
recovery.


LinkedListRanges
-----------------

One of the core driving force for the client side logic is a double linked list
of ranges, which is used to maintain the list of lost packets of the client.

As packets are recieved from server during mcast transfer, any packets lost are
stored into this ll as ranges. Inturn as packets are recovered during ucast
recovery, those specific packet/block id's are removed from the ll.

At the top level, the linked list will maintain reference to the

* start/first node in the list
* end/last node in the list

as well as

* the last added node, in the list.
* the node immidiately before the last deleted (if any) node, in the list

It also maintains a count of total number of nodes and inturn the total/actual
number of values stored/represented in the list indirectly in the shortened
form of ranges.

At the individual node level it maintains the start and end values
respresenting the range being stored in that node. As well as the prev and next
links to the nodes before and after it.


NwGroups
---------

A given group of Server/ServerInstance and a set of clients assigned to that
Server/ServerInstance is given a unique NwGroup id/number. This helps assign a
unique set of ports for that nw test/transfer group.

By default this is 0. Which is good enough if only 1 nw test/transfer is used.

However if multiple nw tests/transfers require to be run parallely, then each
such group of server+clients should be given unique NwGroup Ids.

This also allows a single machine to run multiple instances of server or
multiple instances of client logic if required.


Content Length
----------------

Client comes to know about the total length of the test blocks / file being
transfered based on one of the following events.

* During mcast transfer phase each recieved packet could potentially be the
  last packet wrt the content transfer. There is no seperate marker to indicate
  that it is the last packet.

* McastStop command contains info about the total number of blocks involved in
  the current content transfer.

* URReq command/packet contains info about the total number of blocks involved.

Thus the client can known about the total blocks involved in the transfer and
inturn thus identify any lost packets towards the end of the transfer from
either the mcast phase or the ucast phase.


Save Resume logics
-------------------

In multicast phase both server and client side have logics to save context if
they are forced to quit using SIGINT. And inturn if restarted along with
specifying the saved context file to use, they will restart from where they
left off.

In unicast phase, the client side has logic to save context if forced to quit.
And inturn logic to load a previously saved context and continue from where
things were left off, if asked to do so.

In unicast phase, the server side has a simple save context logic of saving the
list of clients it knows about. Similarly it has logic to load a list of known
clients, if provided by the user.


The Packet structure
----------------------

cur
~~~~

PIReqSeqNum, NwContextId, dummy, TotalBlocksInvolved, ....
DataSeqNum,  NwContextId, dummy, TotalBlocksInvolved, data
URReqSeqNum, NwContextId, dummy, TotalBlocksInvolved, ...

New
~~~~

PIReqSeqNum, NwContextId, 8BitFlag|24BitCtxtver, TotalBlocksInvolved, ...
DataSeqNum,  NwContextId, 8BitFlag|24BitCtxtver, TotalBlocksInvolved, data
URReqSeqNum, NwContextId, 8BitFlag|24BitCtxtver, TotalBlocksInvolved, ...

Client has to use the context version number to decide, whether it should
continue from a previous saved client side context for that nw context id, or
assume that all data it has for that nw context id is no longer valid (bcas new
version) and inturn start recieving full content wrt that nw context id, i.e by
discarding any existing lostpkts info and start with a new lostpkts list which
tells full content is missing.

24BitCtxtVer is as the name suggests 24bit wide and occupy the Lower 3 bytes of
the total 4 byte space available for Flag+CtxtVer.

8BitFlag for now could be None or SaveClientContext. If SaveClientContext is
set then the client on receiving such a packet will save the client context.

savecontext flag should be used by client to save the client side context for
the current session.


.. raw:: pdf

   PageBreak



Stateless Client
==================


the Idea
----------

Want to change the client logic to be stateless in that

a) It can recieve data or commands over mcast or ucast

b) It can respond to URReq from server at any time

c) It can respond to PIReq from server at any time. Client no longer sends out
   PIReq, rather it responds with PIAck

d) All packets from server identify the transfer context. The context consists
   of Nw Context Id and TotalBlocksInvolved.

   1.1) A 4 or 8 byte almost unique id. Could be generated by intermixing parts
      of a hash of the content, so as to generate a byte array of reqd size.
      However care should be taken to ideally ensure that it is different from
      the value used for last different content.

   1.2) A 4 or 8 byte unique id, which identifies a specific test/transfer
      content/context from client perspective.

   2.0) 4 bytes of Total number of blocks involved in the transfer

e) The logic will starts by creating a llLostPkts with info as to
   all packets are missing. Then for each pkt it recieves, it removes
   the same from the llLostPkts list.

To do the same

a) I now create both the mcast and ucast socket at the beginning itself.

b) It inturn calls into a generic / common run routine which handles
   different types of packets as required.


the Advantage
--------------

On the server side one can jump between mcast based transfer to ucast based
recovery as many times as required when the nw test / transfer is going on by
stoping one and restarting the same or the other as required.

Practically this flexibility can be used as follows.

Initially do a mcast based transfer. At the end if a large number of clients
have a hell lot of lost packets, then reuse the mcast transfer once again or
till that time as to get to a situation where most clients have small amount of
lost packets. And at such a time use the ucast based recovery logic.


context
--------

The client can load a specified context when it is started. Or if the
CTXTAUTOLOAD compile time option is enabled it can dynamically switch from the
current context (after updated it by resaving it) to the new context specified
by using a new nw context id in the packets sent from the server.

However if client can't find a appropriate client side context for the nw
context id, seen as part of CTXTAUTOLOAD logic, then it will put the client
into ERROR STATE, for that particular nw context id.


the Arguments
--------------

The client side simpnwmon02 program has the following command line arguments

./simpnwmon02

Mandatory arguments

--maddr mcast_ip --local local_nwif_index local_nwif_ip --file data_file

Optional arguments

[--contextbase pathANDbasename_forcontext2save --context context2load_ifany --nwgroup id --cid clientID]



.. raw:: pdf

   PageBreak



Client
==========

The client side logic is implemented in a single program.

./simpnwmon02 --maddr 230.0.0.1 --local 0 127.0.0.1 --file /dev/null --bcast 127.0.0.255 --nwgroup 2 --contextbase /tmp/newnow --context /tmp/whatelse.lostpackets.quit --runmodes 7 --cid whome

NOTE: In the above example, the client is run on a non default network group id
of 2. So there should be a corresponding server instance running with the
nwgroup id of 2.


the Arguments
--------------

The client side simpnwmon02 program has the following command line arguments

./simpnwmon02

Mandatory arguments

--maddr mcast_ip --local local_nwif_index local_nwif_ip --file data_file --bcast nw_bcast_ip

Optional arguments

[--contextbase pathANDbasename_forcontext2save --context context2load_ifany --nwgroup id --runmodes runmodes --cid clientID]

the local_nwif means the ethernet or wifi interface which connects to the
network on which we want to run the test/data transfer logic.

the local_nwif_index is the index assigned by linux kernel for the used network
interface. It can be got by using ip addr and looking at the index number
specified by it. i.e if it is the 1st nw interface or .... Nth network
interface for which address details are provided by ip addr command.

the local_nwif_ip is the ip address assigned to the network interface which we
want to use.

The local_nwif_index and local_nwif_ip are used as part of the multicast join
using setsockoption. Ideally one is required to provide only one of these two
values.

If local_nwif_index is not being explicitly specified, then pass 0 for it.

If local_nwif_ip is not explicitly specified, then pass 0.0.0.0 for it.

mcast_ip is the multicast group ip address on which to listen for data / test
packets.

data_file is the file into which recieved data should be saved.

nw_bcast_ip is the network broadcast address into which PIReq packets should be
sent.

context2load_ifany is a optional parameter. This is required to be given, if
one wants the program to resume a previously broken transfer in ucast or mcast
phases. Ideally It should be the file into which the program had saved the
context, when it was force exited previously by sending a SIGINT (ctrl+c)
signal. Default value is NULL (ie dont load any context)

pathANDbasename_forcontext2save is a optional parameter. This is the path and
the base part of the filename to be used for any context files generated by the
program. Default value is /tmp/snm02.

nwgroup id a optional parameter. This helps a given set of clients and its
corresponding server to communicate with one another, independent of other
possibly parallel groups. Default value 0.

runmodes a optional parameter specifies which and all phases of the program
should be run. The values mentioned below can be or'd together, if more than
one phase requires to be run. Default value is 7 (i.e run all the 3 phases).

* 1 represents mcast transfer,
* 2 represents ucast pi,
* 4 represents ucast recovery.
* 65536 - a special value - represents auto mode, where actual value is decided
  based on DoneModes saved in context file being loaded. If no DoneModes in
  context file then runmodes will be set to 7.

clientID is a string representing any given specific client. It is 16 chars
long over the network. However don't assign a id/name larger than 15 chars.
This is passed on to the server as part of the PIReq packet from the client.


Client's context
------------------

It contains

* list of lost packet ranges

* MaxDataSeqNumGot & MaxDataSeqNumProcessed

* DoneModes

Two context files

* When ever the program is asked to quit thro SIGINT

* At the end of mcast phase


the Logic
-----------

The 1st phase of the logic consists of mcast transfer. During this phase it
keeps track of the recieved and lost packets in sequence, as well as saving the
recieved data into corresponding location in the data file specified.

If no packets are recieved for a predefined long time, then the client will
rejoin the mcast group (i.e drop and join) just to be on safe side. This is
done in case if one is on a wifi network and the connection drops and
reconnects, and this if in turn triggers the access point to drop the client
from its mcast group client list. In this case the rejoining should make the ap
re-add the client to the mcast group client list.

If and when it recieves a mcast stop command, it exits the mcast phase. It also
will come to know about the total blocks involved in this file/test transfer.

Next the client tries to notify any server that may be listening, about the
client's presence in the network, as well as to know who the server is. Even
thou both server and client go thro the PI phase, the logics are setup such
that a failure in PI phase doesn't impact the over all flow. The total of
number of lost packets wrt the client is also informed to the server as part of
the PIReq packet.

The logic goes into a unicast recovery phase, where it listens for any requests
from server about lost packets. In turn when the server requests, the client
sends the top N number of lost packet ranges it has. Parallely if it recieves
any data packets, which it didn't have before, it will save the same into the
data file. The total number of lost packet ranges and inturn the total number
of lost packets represented thro these ranges is also sent to the server as
part of the URAck packet. The server informs about the total number of blocks
involved in the current transfer to the client as part of URReq packet.

NOTE: During ucast phase, by default the logic is implemented to ignore nw
errors and inturn continue in a suitable manner, which doesn't impact the
overall logic/flow much.


Save & Resume
---------------

If one forces the program to quit when it is in the middle of a transfer, by
sending a SIGINT. Then the program irrespective of whether it is in mcast
phases or ucast phases, will save the current list of lost packets to a
predefined location. Also some other important variables/data/info which
provide context to the current transfer is also saved.

This info can be used to resume the transfer and recieve remaining data if any
as well as recover remaining or lost packets. A basic resume logic has been
added, which allows recovering when the client was stopped in the middle of
either the mcast phase or the ucast phase.

ToDO: A more full fledged context requires to be saved, so that when one
resumes, even the network performance related info is also recovered esp wrt
the mcast transfer interruption.

NOTE: A ctrl+c will generate SIGINT if client is being run directly on a
console as the foreground process.

AutoAdjust RunModes
~~~~~~~~~~~~~~~~~~~~

THere is a compile time option to enable auto adjusting of the runmodes based
on the saved donemodes, as part of context loading. This option is enabled by
default. For this logic to apply, additionally user is also required to specify
that --runmodes = 65536 (represents auto) through the commandline.

DoneModes tracks as to what and all phases of the transfer are already
done/skipped. This inturn is saved into the context file.

THis ensures that if the context file passed to the program was the one
generated by the program during a previous run, when it got forcibly quit using
SIGINT, then the program will automatically resume in the correct phase,
without user having to worry about it, provided the user set the runmodes into
auto mode.

If runmodes is set to auto, and there is no DoneModes in the context file being
loaded, or if there is no context file, then runmodes gets reset to 7.

Caution
''''''''
However if a long time has passed between when the program was forced to quit
and now when it is being resumed, then the server might have already finished
with the phase which was active when the client program quit, so it may get
into the wrong phase in such a situation. In such situations one should
manually edit the DoneModes entry in the context file, before passing it to
resume OR better still the user should explicitly specify the runmodes thro the
commandline.

The above caution is mainly applicable when only client is being restarted.
However if even the server side ucast program is being restarted along with all
the clients, then one can run the clients with --runmodes 6 (or even 7 will do,
as server pi logic will automatically send out mcast_stop if the client hasn't
sent any PIReq packets in a given time).

Ideal case usage
'''''''''''''''''
With this ideally, in the normal case, when starting the program on powering
on, the runmodes should be specified as 7 or not specified at-all, in which
case again it defaults to 7. This is equivalent to run all modes/phases.

Where as if the program is being restarted because the previous instance got
forcibly quit, then in this case the runmodes should be specified as auto, so
that it will get autoadjusted to the right phases based on the donemodes saved
in the context file when the program quit previously.

So we could use a helper script like this

.. code-block:: sh

   # runmodes = 7 means run all modes
   # runmodes = 65536 means autoadjust runmodes from saved context donemodes

   theRunModes=7
   while True; do
     ./simpnwmon02 --runmodes $theRunModes .....
     theRunModes=65536
   done


.. raw:: pdf

   PageBreak



Server
========

The server side logic is implemented as part of two different programs.

the Programs
--------------

mcast phase
~~~~~~~~~~~~

The first takes care of the multicast phases. This program can be stopped and
restarted, provided one uses --startblock to explicitly specify where to start
in the overall transfer or use --context to specify the saved context generated
when the program was stopped.

ucast phase
~~~~~~~~~~~~

The second takes care of the unicast phases. If required this unicast related
script can be called more than once, provided a context file is passed to it,
with the list of remaining clients with lost packets.

Even if the full list of know clients is passed to the 2nd invocation of the
ucast recovery program / script, the logic will handle all corner cases
properly. Because even if there are clients with fully transfered contents, if
they are running, they will inform the server that they dont have any lost
packets; and if they are not running, the server will automatically timeout wrt
such clients (the program will take more time than ideal, otherwise no other
issues).

the Arguments
~~~~~~~~~~~~~~

common arguments
''''''''''''''''''

--maddr

--file

--testblocks

--Bps

--context

--nwgroup

--dim

--datasize

--ncid


mcast specific arguments
'''''''''''''''''''''''''

--startblock

--simloss

ucast specific arguments
'''''''''''''''''''''''''

--laddr

--slow



the Context file
------------------

mcast phase
~~~~~~~~~~~~

The context file identifies that it relates to mcast and contains the last
packet/block id sent as well as the total number of content blocks involved.

MCAST:LastSent:TotalInvolved

ucast phase
~~~~~~~~~~~~~

THis is a file used by the unicast phase server program, to get the list of
clients it should try to help wrt recovering their lost packets.

A text file having the tag <clients> in a line. Followed by lines containing
the ip addresses of the clients, one per line. Followed by </clients> in a
line.


for Firewall
--------------

The nw port usage is as follows if NwGroup is 0 (the default)

a) 1111 - Multicast Server to Clients data push
b) 1112 - Nw Broadcast PIReq from Client to Any listening Server
c) 1113 - Unicast PIAck from Server to Client

However if there are NwGroups with id/num other than 0, then use following to
identify the port to be enabled.

PortUsed = BasePort + 5*NwGroupId


status
-------

In addition to the status prints on the console, the logics also save important
summary progress update info periodically to /tmp/snm02.srvr.status.log


.. raw:: pdf

   PageBreak



Examples
==========


Example for actual testing
----------------------------

Client Side Initialy
~~~~~~~~~~~~~~~~~~~~~

Client> ./simpnwmon02 --maddr 230.0.0.1 --local 0 10.0.2.11 --file /path/to/datafile --bcast 10.0.2.255 --contextbase /path/with/basefilename

Server Side
~~~~~~~~~~~~~

Server> ./hkvc-nw-send-mcast.py --maddr 230.0.0.1 --file /path/to/file_to_send

Possibility1 (Prefered) ==>

Server> ./hkvc-nw-recover.py --maddr 230.0.0.1 --file /path/to/file_to_send --context /path/to/file_with_list_of_all_known_client_ips FOLLOWED_BY_IF_REQUIRED

Server> ./hkvc-nw-recover.py --maddr 230.0.0.1 --file /path/to/file_to_send --context /path/to/file_with_list_of_all_known_or_remaining_client_ips

Possibility2 ==>

Server> ./hkvc-nw-recover.py --maddr 230.0.0.1 --file /path/to/file_to_send AND_OR

Server> ./hkvc-nw-recover.py --maddr 230.0.0.1 --file /path/to/file_to_send --context /path/to/file_with_list_of_all_known_or_remaining_client_ips

Client Side Resume
~~~~~~~~~~~~~~~~~~~~~

Explicit control
''''''''''''''''''

If one wants to control the phase to resume into, then use one of the below.

If the client was force quit in the middle of a multicast phase, then to resume run the below

Client> ./simpnwmon02 --maddr 230.0.0.1 --local 0 10.0.2.11 --file /path/to/datafile --bcast 10.0.2.255 --runmodes 7 --context /path/to/saved_contextfile

If the client was force quit in the middle of a unicast phase, then to resume run the below

TO run both UCast PI and UR phases

Client> ./simpnwmon02 --maddr 230.0.0.1 --local 0 10.0.2.11 --file /path/to/datafile --bcast 10.0.2.255 --runmodes 6 --context /path/to/saved_contextfile  OR

TO run only the UCast UR phase

Client> ./simpnwmon02 --maddr 230.0.0.1 --local 0 10.0.2.11 --file /path/to/datafile --bcast 10.0.2.255 --runmodes 4 --context /path/to/saved_contextfile

The default /path/to/saved_contextfile will be /tmp/snm02.context.quit, however if --contextbase was given then updated path and name suitably.

Auto Adjust
'''''''''''''

If one wants the program to auto decide as to which phase it should resume into then run as below

Client> ./simpnwmon02 --maddr 230.0.0.1 --local 0 10.0.2.11 --file /path/to/datafile --bcast 10.0.2.255 --context /path/to/saved_contextfile --runmodes 65536



Example for testing using VM
------------------------------

The below example assumes autogenerated testblocks are used instead of a actual file

Client side initially
~~~~~~~~~~~~~~~~~~~~~~

On Client run

Client> ./simpnwmon02 0 230.0.0.1 10.0.2.11 /dev/null 10.0.2.255

Server side
~~~~~~~~~~~~~

On Server run, these two commands one after the other

Server> ./hkvc-nw-send-mcast.py --maddr 230.0.0.1 --testblocks 50000 --simloss

Possibility1 ==>
Server> ./hkvc-nw-recover.py --maddr 230.0.0.1 --testblocks 5000 AND_OR
Server> ./hkvc-nw-recover.py --maddr 230.0.0.1 --testblocks 5000 --context /path/to/file_with_list_of_client_ips

Possibility2 ==>
Server> ./hkvc-nw-recover.py --maddr 230.0.0.1 --testblocks 5000 --context /path/to/file_with_list_of_all_known_client_ips FOLLOWEDBY_IFREQUIRED
Server> ./hkvc-nw-recover.py --maddr 230.0.0.1 --testblocks 5000 --context /path/to/file_with_list_of_all_known_or_remaining_client_ips

If required could Use slow mode ==>
Server> ./hkvc-nw-recover.py --maddr 230.0.0.1 --testblocks 5000 --slow

Client side, if interrupted
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If you want the client program to auto resume into the right phase, then run
Client> ./simpnwmon02 --maddr 230.0.0.1 --local 0 10.0.2.11 --file /dev/null --bcast 10.0.2.255 --context /path/to/saved_contextfile --runmodes 65536

If the client was force quit in the middle of multicast phase, then to resume run the below
Client> ./simpnwmon02 --maddr 230.0.0.1 --local 0 10.0.2.11 --file /dev/null --bcast 10.0.2.255 --context /path/to/saved_contextfile --runmodes 7

If the client was force quit in the middle of unicast phase, then to resume run the below
Client> ./simpnwmon02 --maddr 230.0.0.1 --local 0 10.0.2.11 --file /dev/null --bcast 10.0.2.255 --context /path/to/saved_contextfile --runmodes 6

The default /path/to/saved_contextfile will be /tmp/snm02.context.quit


.. raw:: pdf

   PageBreak



Release Notes / Thoughts during some of the releases
#####################################################

v20190204IST0244
===================

Introduce a minimal PI even at the end of mcast transfer.

More refined PI status & console logging.

Client understands if a SaveClientContext request from server is a fresh one or
a repeat, as part of trying to communicate with non-responsive clients. And
inturn only save context if it is a fresh one.


v20190203IST1704
==================

Client and Server updated to manage a 1 Byte Flag sent from srvr to client.

Currently this is inturn used to send save client context request to clients
from the server, when the server takes a 1 minute relax break in the middle of
mcast transfer, by sending a PIReq packet which has the appropriate flag set.

This ensures that even if the client crashes or is powered off unknowingly or
by uncontrolled things, there will always be a periodic client side context
being saved once every 15 minutes roughly. So the client will only loss any
data it had recieved in a window of max 15 minutes, which either way it will
recover, when the client is resumed (by asking server to resend those pkts,
as part of recovery process).

Fix a oversight in mcast transfer program, wrt binding of socket to local port
to recv PIAcks.

Cleanedup PI logging.


v20190202IST2336 - v2.00 Beta
==============================

Made PI phase more aggressive by reducing the time the server waits after
sending PIReq and inturn use the gained time to increase the number of times
PIReq is sent. Practically found that 30 clients were responding to PIReq
within a 1 second window, so reduced the default wait from 120 seconds to 30
seconds now.

Added PI Phase to mcast phase logic also. Initially before starting mcast
phase, the logic will do a medium length PI phase. And later in the middle of
mcast phase, once every predefined amount of time it will trigger a small less
aggressive pi phase.

PI phase logic now instead of doing only 1 attempt, if no client list is
provided, will try for the maximum number of times specified. While if a client
list is given, then similar to before, once all clients have handshaked, it
will come out of PI phase.

PI Phase function, is more controllable now by the caller, wrt its logic,
interms of how many attempts should be tried and how much time to wait during
each attempt for clients to handshake back.

Now one can explicitly specify which local interface should be used for mcast
sending on the server side by passing a --laddrms argument. This interface
inturn could be different from the local interface used for recieving udp
packets.

Added support for NwContext Versioning. One can specify the version of the
Nw Context Id content being currently tested / transfered over the network,
by using the --ncver argument on the server side. In turn the client will
verify that the version hasn't changed between the last time it handled
that given Nw Context Id's content and the new content with matching
NwContextId recvd over the network. If both match, then the nw session is
continued, else the client will assume that user / server wants to send a
new version of the given Nw Context Id's content and handle the same as
required. NOTE: At same time the context auto loading logic could also
parallely trigger, which inturn also leads into this immidiately after the
context id related switch is done.

With this the client side logic is capable of switching between predefined
content id's as well as new versions of the same automatically without
requiring any intervention from the user on the client side. The only thing
that is required is that the Client side context for each of these predefined
NwContextId's be predefined on the client side once.


v20190201IST2345 - v2.00 alpha
=================================

Have enabled auto context switching on the client side based on the nw context
id it recieves in the packets. However this requires that one has already
created / saved client side context files which correspond to the different nw
context id's being used in the network. THese client side context files should
contain DataFile defined such that they map to different unique files
corresponding to each unique nw context id.

On the server one can use the --ncid argument to specify the nw context id to
be used for a given test / transfer. It is supported by both the mcast and
ucast scripts on the server side.


v20190201IST1857
=================

Have updated the stateless client such that it saves its context and loads its
context properly. This also includes the ContextFileBase and DataFile specified
when a new context was saved originally.

Inturn when loading a context the value saved in the saved context file
overrides the ones specified on the commandline.

Also the skeleton to help auto switch context on client side, based on any
different nw context id seen in the network packet is implemented. However as
there is a corner case to be fixed wrt the situation where the new nw context
id seen over the network in the middle of a transfer being a totally new nw
context, this logic is not enabled by default. Currently any change in nw
context id without restarting the client logic, will lead to the client
ignoring those new nw context id related packets.

NOTE: The idea of this logic, is that one could have the client logic
automatically track different content files / partitions as the server changes
the nw context id, without the client side having to do anything else at one
level. So one can transition between multiple context files / partitions
transparently while at same time handling packet recovery properly
corresponding to that particular test / transfer content.

Also the old Done/Run Modes and other logics wrt State based Client have been
removed and logic updated suitably, as Stateless client doesn't use these
mechanisms, but as alternate semantic.


v20190131IST0016
===================

And the 2019_02xy_v2.x branch, which is the experimental stateless client logic
and its corresponding server side logic.

Basic stateless client logic along with supporting server side code has been
implemented to get the basic flow working on both server and client side.

The basic core logic has been implemented on both the client and server side,
so that one could use this to test / transfer files. And basic testing seems to
indicate it is working as expected.

However it has not been profiled from nw / system performance perspective, as
well as it has not been fully verified from the perspective of not introducing
any holes in the file, in remote corner cases. THese aspects require to be
tested.

Also the saving and restoring of context on the client side. Cleanup of
server and client side logics wrt this new stateless based flow needs to
be done.

Also multiple parallel nw instances related logics have not yet been properly
updated.

The advantage of this logic is that one could use either mcast or ucast for
recovery phase, based on the amount of overall losses seen. If losses are too
many across board (i.e across a lot of clients), then re-run the send-mcast
script again. However if the losses are relatively low then ucast based
recovery script is better.

And also this stateless flow makes the over-all logic simpler at one level.
Also lot of common functionality is now naturally consolidated into a single
location, among other extensive simplifications wrt state handling and context
correctness guarenteeing.


v20190130ISTXYZA - v1.03
=========================

Now mcast Rejoin only if no mcast packets for predefined interval, by default.
However if one requires the logic to rejoin always once every predefined
interval period then one can pass a compile time define to enable the same.


v20190129IST1647 - v1.02
==========================

Unicast recovery phase now ignores network errors and continues with the logic
in a suitable manner by default.

This version removes the nw error related exits in the unicast recovery phase.
So even if the network connection fails during this phase, the logic will
continue to persist, with the hope that network connection will be restored by
the client system's network managing logic.

Multicast ALL property is no longer set for the multicast socket.

NOTE: If network fails during PI phase for more than 10 minutes, the logic will
quit pi phase and go into ucast recover phase.This is fine, as long as the
server nw-recover script is started with a context file containing all clients
in the network.


v20190128IST1259 - v1.01
=========================

Have added mcast rejoin (i.e drop first followed by join) functionality to
mcast phase logic on the client side, which gets triggered if there is no mcast
data for a predefined time (currently it will trigger once every 5 to 6 minutes
of inactivity).

Also this mcast drop and join, even if it fails, it will log the same info and
continue remaining in mcast phase. THis is to hopefully help ensure that even
if the network is down when the client tries to rejoin, it should continue to
remain in mcast phase. IN this case, after waiting for another additional 5 to
6 minutes of inactivity, it will try to rejoin again.

A WiFi AP will normally drop a client from the multicast group client list, if
the client disconnects from the AP. When the client's network manager
reconnects to the AP, it wont get re-added back automatically to the multicast
group client list by the AP. So even thou a multicast client logic is still
active and running, it will no longer recieve multicast packets, because the AP
will no longer forward multicast packets to it. This is the reason why this
rejoin logic is required.


v20190119IST1542 - v1.00
=========================

Fix the oversight wrt unwanted capitalisation of the clientsDB keys in status
module wrt pi status logging.

Print pktid as part of throughput print during mcast transfer, to better track
progress for users.

Print cur pktCnt as part of the throughtput print during ucast recovery
transfers, again to better track progress by users.

Print cur Ref/Block count as part of the periodic check-image's progress print.

Added option to specify a clientID on the client side using --cid argument.
This will be passed to server thro PIReq packet.

nw-send-mcast now saves the context even on a successful exit.


v20190119IST0931 - rc6
========================

nw-send-mcast now allows one to specify from where in the overall nw
test/transfer one should start transfering, i.e from the 0th block or a
specified (through commandline arg --startblock) Nth block.

This allows one to manually stop and restart mcast transfer, as required.

nw-send-mcast now has a optional --context argument. If it is specified, and
inturn if it contains a previously saved mcast context, the mcast transfer will
continue from where it was left previously. If the specified context file is
non-existant or empty, then the mcast transfer will start from the beginning.
And inturn in either case, if a user forces the program to quit, it will save
the context into this specified file.

If no context argument is given, and user forces the program to quit using
SIGINT, then it will save the context into a predefined location
/tmp/snm02.srvr.context.mcast

Fixed a oversight wrt 'cnt' during generation of ucast_pi summary status. Also
now Name and LostPkts info got from clients during PI phase is properly
captured in the summary status file.

Cleaned up progress update logging in the status file.

Notes updates and cleanups.


v20190118IST1010 -  rc5
========================

URReq packet from server now includes the TotalBlocksInvolved. This ensures
that If a user interrupts the client in the middle of mcast transfer and then
forces it to resume into unicast phase, the logic now automatically accounts
for packets lost from the time of mcast transfer interruption to end of mcast
transfer.

PIReq packet from the client now also includes the TotalLostPkts wrt the
client. For now the server just prints out that info, so that the user can get
a rought idea of how the network has performed in general and wrt each clients.
In future it could be used for prioritising or deciding mode of recovery or ...

check-image script/program now prints the missing blocks in a testblocks based
transfer, as ranges of lost blocks, instead of printing id of each individual
lost block. Also if a block seems to be out of sequence, then a warning line
will be printed.

A status module added to help with collecting important progress status at a
predefined location. All phases of the logic i.e mcast transfer, mcast stop,
ucast pi and ucast ur phases now use status module's related functions to share
their respective progress updates.

/tmp/snm02.srvr.status.log contains summary progress updates across all phases
on the server.


v20190116IST2323 - rc4
========================

NwGroup support added to server side programs also now. With this now both
server and client support nwgroups concept. With this one can have multiple
parallel independent nw test/transfer sessions running on the network, at the
same time.

Now the Client program --runmodes argument can take a additional value called
auto represented by 65536. If this is given and then if a context file is being
loaded, so as to resume a previously interrupted nw transfer session, then the
client program will automatically decide the appropriate runmodes/phases to be
enabled for this run. So the user no longer as to worry at what phase the
client program was forced to quit, the program will save this information as
part of its saved context and when this saved context is loaded into a new
instance of the client, it will automatically go into the right phase/mode of
the transfer.

NOTE: However if a sufficiently long time as passed between interruption and
resumption of the client side program, then it is better to explicitly set /
specify the runmodes to be enabled for this run in the commandline, after
looking at the server side's current phase.


v20190116IST15XY - rc3
========================

This version allows the client side logic to be resumed, even if it was
interrupted in the middle of the multicast transfer. And in this case the
--runmodes should be 7 (and not 6 or 4, which is used for ucast phase
resumption).

NOTE: The network transfer performance related info is not currently saved and
restored between interruption-resumption cycle. So the nw transfer performance
data will contain info related to the resumed section of the transfer only.

Do read the notes at the root, to understand the logic and usage better.


v20190115IST02XY - rc2
=======================

Attached is a updated version with following main changes

a) All nw program related variables moved into a single context. And wrapper
funcs added to use this new context, as required.

b) Added a nwgroup argument, which helps have multiple parallel nw
tests/transfers running on the network, as well as wrt multiple server
instances/client instances running on a given machine. Currently this support
has been fully implemented on client side. TODO1: In next release it will be
also added to the server side logic.

TODOX:
Later MaxSeqNumSeen till a given moment will also be saved as part of this
context. And then saving and restoring of the nw context will be added. This
will allow one to implement mcast resume on the client side if required in
future.


v20190114IST1923 - rc1
=======================

Mainly a cleanup and fine grained control related updates wrt client logic.

The client now uses descriptive tags to identify the arguments being specified.
Running the client without arguments will give the details. A sample client run
will be

./simpnwmon02 --maddr 230.0.0.1 --local 0 10.0.2.11 --file /path/to/datafile --bcast 10.0.2.255 --contextbase /path/to/contextfilebasename

For some reason if client was stopped in the middle of unicast recovery then to
resume within ucast recovery run

./simpnwmon02 --maddr 230.0.0.1 --local 0 10.0.2.11 --file /path/to/datafile --bcast 10.0.2.255 --context /path/to/saved_contextfile --runmodes 4

NOTE: that normal running requires --contextbase, while resuming requires
--context. Also resuming requires --runmodes 6 (if server still in PI phase) or
--runmodes 4 (if server already in UR phase or even if in PI phase, this will
always work).

Also when done with mcast, now it saves a lost packet ranges context file. This
is independent of the quit related lost packet ranges context file, which will
be created if the program is forced to quit with a SIGINT.

Just to be clear:

If for some reason one had to stop the client in the middle of unicast recovery
by sending it a SIGINT. Then while resuming it

Irrespective of whether the server is in unicast PI phase or unicast UR phase,
the client can be resumed with --runmodes 6 or --runmodes 4, and everything
will work fine.

However if we want to resume and resync in a efficient manner then

If server in ucast PI phase, then start client with --runmodes 6
if server in ucast UR phase already, then start client with --runmodes 4


v20181228
===========

There is some odd holes seen in the data file after both mcast and ucast are
finished successfully. Need to cross-check this later.

Tried changing from FileOutputStream to RandomAccessFile in-case if its that
FileOutputStream doesn't allow selective writing into a existing file, but that
doesnt seem to have solved it, need to test the RandomAccessFile after removing
the data.bin file on the target and see how a fresh transfer with
RandomAccessFile works out.

Also on testing on a actual physical android target, found that if the packet
data size is at something like 8 bytes or so, the Android Java based GUI is
picking up the packets, but if I increase the data size to 32 or above, it
doesn't seem to be recieving the packets.

v20181225+
==========

The nw port usage is as follows

a) 1111 - Multicast Server to Clients data push
b) 1112 - Nw Broadcast PIReq from Client to Any listening Server
c) 1113 - Unicast PIAck from Server to Client


So if using Android AVD for testing remember to redir both 1111 as well as 1113

i.e telnet localhost 5554
NOTE: assuming it is the 1st avd started
auth value_required_to_authenticate
NOTE: got from .emulator.... file in the users home dir
redir add udp:1111:1111
redir add udp:1113:1113
redir list

Also if using AVD, then in GUI remember to set the PIInitialAddr to 10.0.2.255
in the given unicast related edittext.



v20181223
===========

Data Thread synchronisation
------------------------------

* Failure - UseData before FillData

Producer->Acquire->FillData->Loop
Consumer->UseData->Release->Loop

* Failure - Race, FillData before UseData is finished

Producer->FillData->Acquire->Loop
Consumer->Release->UseData->Loop

3Locks&Buffers
1,2,3,0-1
0,0,0,1=XXXXXX

* Ok - SemCount 1 or more less than Total Buffers

Producer->FillData->Acquire->Loop
Consumer->Release->UseData->Loop

3B(2L)
1-1,2-2,3-1,
0-0,1-1,2-2,

B1-L1,B2-L2,B3-L1
L0-B0,L0-B0,L1-B1

But will require dummy producing to flush out data in deltaOf(buf-lock) buffers
at the end, when actual producing is done.

TODO
-------

01) Currently Data is copied from a fixed buffer in AsyncTask to the data
buffer in DataHandler, avoid this and use the data buffer in DataHandler
directly.

02) Currently only a predefined (set to 1 currently) monitored channel is
logged as well as its data saved.

However if required Update the Logging and Data saving logic to work across
multiple channels.  i.e Each channels log and data should be saved to seperate
log and data files.

03) There is a issue with the 1st packet with seq number 0 being considred as a
olderSeqs, fix this corner case.

04) Add logic to use unicast to recover the packets lost during multicast.


v20181220
===========

hkvc-nw-test script new argument

--file file_to_send

Target java.net.multicast logic

Now it logs lost packets into lost.log file in the applications' directory on
external storage


v20181207IST1005
=================
hkvc-nw-test script arguments

--Bps 2000000 will set the throughput to 2M bytes per second

--datasize 1024 will set the packet size to 1K. The actual packet will be
4bytes+1K, where the 4 bytes correspond to 32bit seqNum in little-endian
format.

--dim 17, tells as to after how many packets are sent the throttling delay if
any should be applied.

--port 1111, tells that udp packets should be sent to port 1111

by default the logic is programmed to send packets to 127.0.0.1. By changing it
to a multicast ip address, one should be able to send to multicast groups
ideally. Have to cross-check the multicast packet sending requirements once,
but I feel that we dont require any special settting of socket for sending
multicast packets, while reception will require joining of the multicast group.
If this vague remembering of multicast behaviour that I have is correct, then
just changing the address in the program will allow using of this simple
pythong script to test multicast transfer behaviour to some extent.


v20181204
============

Now If only one mcast channel is being monitored, then it assumes that it could
be a high throughtput channel, so it will update the progress wrt monitoring in
the gui, only once every 10 iterations thro the monitoring loop.

However if more than 1 channel is being monitored at the same time, then as the
program currently doesn't provide a efficient way of handling this case, it
assumes that the channels are not high througput ones, and or the user is not
interested in getting accurate detailed monitored info like num of disjoint
seqNums noticied or num of times the seqNum jumped backwards etc. So it updates
the progress of monitoring in the GUI for each iteration thro the monitoring
loop.


v20181202
============

TODO1:

Verify if any buffering occurs if lot of packets are recieved on a given
channel.  Because in a given loop I read only 1 packet from a given channel and
wait for timeout or reception (again read only 1 packet, even if more are
there) of data on other channels.

And see the impact of the same practically.

NOTE1:

Supports max of 10 MCast channels i.e MCastGroupIP+Port.
It waits for upto 50msecs before timing out wrt each channel being monitored.
So if there are 10 channels being monitored and 9 of them don't have any data
then it will take 450+timeToReadDataFromTheSingleChannelWIthData msecs for 
each packet of data read from the alive channel.

So this will work for monitoring upto 10 channels with activitiy of 1 or 2
packets per second.

However if the data throughput is heavy, then monitor that single channel only 
to avoid lossing data packets due to overflow wrt buffers allocated by kernel
for the channel.

NOTE2:

ONe can specify different delay counts wrt when to treat delay in data activity
on a channel to be critical to mark it red. If only 1 channel is monitored,
then the delay count corresponds to delaycount*50msec of delay. However if more
than 1 channel is monitored, then the delay count to time mapping is more
complicated and dependent on data activity in realtime across all those
channels. Rather the delaycount can be treated as how many times the
applications checked to see if there is any data for a given channel and then
timedout.

TODO2: If I account timeout wrt other channels also, for each given channel,
then the delay count mirrors the actual time lost more accurately, and the 
delaycount*50msec can still be valid to a great extent. However the current
logic doesn't do this. Also this logic would assume that any channel which
reads data instead of leading to a timeout, will read the data at a very fast
rate which is in the vicinity of within a msec or so. Else the delta between
the actual delaycount based time calculation and real wall clock time will
increase.



Results
#############

201902
========

20190204-XYZ-ENDSetup-OutsideABCHanger
---------------------------------------

20 Mbps
~~~~~~~~

Single Aruba AP active, test transfer at 2 MBps (20 Mbps)

12 Clients => 1% losses
18 Clients => 10% losses
24 Clients => 30% losses

This is the result with checking after 30MB of total data transfer.

When tested after 2GB data transfer

24 Clients => 35% losses

MCAST_TX:1703680/3096576
PI_REQ:239.199.199.199:0:0/1
SET:UCAST_PI:ClientCnt:24
UCAST_PI:IP=999.999.85.14:LP=2005576:C=1:N=b'v20190202iAMwho\x00'
UCAST_PI:IP=999.999.85.7:LP=1980509:C=1:N=b'v20190202iAMwho\x00'
UCAST_PI:IP=999.999.84.252:LP=1949936:C=1:N=b'v20190202iAMwho\x00'
UCAST_PI:IP=999.999.85.15:LP=2009644:C=1:N=b'v20190202iAMwho\x00'
UCAST_PI:IP=999.999.85.4:LP=-1:C=0:N=UNKNOWN
UCAST_PI:IP=999.999.85.3:LP=1958748:C=1:N=b'v20190202iAMwho\x00'
UCAST_PI:IP=999.999.84.254:LP=1987764:C=1:N=b'v20190202iAMwho\x00'
UCAST_PI:IP=999.999.84.253:LP=1993648:C=1:N=b'v20190202iAMwho\x00'
UCAST_PI:IP=999.999.85.11:LP=2018816:C=1:N=b'v20190202iAMwho\x00'
UCAST_PI:IP=999.999.85.10:LP=1964910:C=1:N=b'v20190202iAMwho\x00'
UCAST_PI:IP=999.999.85.2:LP=1996320:C=1:N=b'v20190202iAMwho\x00'
UCAST_PI:IP=999.999.85.18:LP=1946035:C=1:N=b'v20190202iAMwho\x00'
UCAST_PI:IP=999.999.85.16:LP=1937697:C=1:N=b'v20190202iAMwho\x00'
UCAST_PI:IP=999.999.85.5:LP=2027791:C=1:N=b'v20190202iAMwho\x00'
UCAST_PI:IP=999.999.84.255:LP=2013868:C=1:N=b'v20190202iAMwho\x00'
UCAST_PI:IP=999.999.85.6:LP=1976525:C=1:N=b'v20190202iAMwho\x00'
UCAST_PI:IP=999.999.85.13:LP=2023388:C=1:N=b'v20190202iAMwho\x00'
UCAST_PI:IP=999.999.85.9:LP=2009855:C=1:N=b'v20190202iAMwho\x00'
UCAST_PI:IP=999.999.85.8:LP=1954707:C=1:N=b'v20190202iAMwho\x00'
UCAST_PI:IP=999.999.85.12:LP=1967674:C=1:N=b'v20190202iAMwho\x00'
UCAST_PI:IP=999.999.85.17:LP=2037201:C=1:N=b'v20190202iAMwho\x00'
UCAST_PI:IP=999.999.85.1:LP=1943042:C=1:N=b'v20190202iAMwho\x00'
UCAST_PI:IP=999.999.85.0:LP=1972756:C=1:N=b'v20190202iAMwho\x00'
UCAST_PI:IP=999.999.85.19:LP=2032296:C=1:N=b'v20190202iAMwho\x00'
PI_REQ:239.199.199.199:1:0/1
SET:UCAST_PI:ClientCnt:24
UCAST_PI:IP=999.999.85.14:LP=2005576:C=2:N=b'v20190202iAMwho\x00'
UCAST_PI:IP=999.999.85.7:LP=1980509:C=2:N=b'v20190202iAMwho\x00'
UCAST_PI:IP=999.999.84.252:LP=1949936:C=2:N=b'v20190202iAMwho\x00'
UCAST_PI:IP=999.999.85.15:LP=2009644:C=2:N=b'v20190202iAMwho\x00'
UCAST_PI:IP=999.999.85.4:LP=2004009:C=1:N=b'v20190202iAMwho\x00'
UCAST_PI:IP=999.999.85.3:LP=1958748:C=2:N=b'v20190202iAMwho\x00'
UCAST_PI:IP=999.999.84.254:LP=1987764:C=2:N=b'v20190202iAMwho\x00'
UCAST_PI:IP=999.999.84.253:LP=1993648:C=2:N=b'v20190202iAMwho\x00'
UCAST_PI:IP=999.999.85.11:LP=2018816:C=2:N=b'v20190202iAMwho\x00'
UCAST_PI:IP=999.999.85.10:LP=1964910:C=2:N=b'v20190202iAMwho\x00'
UCAST_PI:IP=999.999.85.2:LP=1996320:C=2:N=b'v20190202iAMwho\x00'
UCAST_PI:IP=999.999.85.18:LP=1946035:C=2:N=b'v20190202iAMwho\x00'
UCAST_PI:IP=999.999.85.16:LP=1937697:C=1:N=b'v20190202iAMwho\x00'
UCAST_PI:IP=999.999.85.5:LP=2027791:C=2:N=b'v20190202iAMwho\x00'
UCAST_PI:IP=999.999.84.255:LP=2013868:C=2:N=b'v20190202iAMwho\x00'
UCAST_PI:IP=999.999.85.6:LP=1976525:C=2:N=b'v20190202iAMwho\x00'
UCAST_PI:IP=999.999.85.13:LP=2023388:C=2:N=b'v20190202iAMwho\x00'
UCAST_PI:IP=999.999.85.9:LP=2009855:C=2:N=b'v20190202iAMwho\x00'
UCAST_PI:IP=999.999.85.8:LP=1954707:C=2:N=b'v20190202iAMwho\x00'
UCAST_PI:IP=999.999.85.12:LP=1967674:C=2:N=b'v20190202iAMwho\x00'
UCAST_PI:IP=999.999.85.17:LP=2037201:C=2:N=b'v20190202iAMwho\x00'
UCAST_PI:IP=999.999.85.1:LP=1943042:C=2:N=b'v20190202iAMwho\x00'
UCAST_PI:IP=999.999.85.0:LP=1972756:C=2:N=b'v20190202iAMwho\x00'
UCAST_PI:IP=999.999.85.19:LP=2032296:C=2:N=b'v20190202iAMwho\x00'
MCAST_TX:1734656/3096576


40 Mbps
~~~~~~~~~

6 Clients, 3GB data = ie Only 2.5% losses

MCAST_TX:3066624/3096576
MCAST_TX:3096576/3096576
PI_REQ:239.199.199.199:0:0/1
SET:UCAST_PI:ClientCnt:6
UCAST_PI:IP=999.999.84.253:LP=-1:C=0:N=UNKNOWN
UCAST_PI:IP=999.999.84.254:LP=56130:C=1:N=b'v20190202iAMwho\x00'
UCAST_PI:IP=999.999.85.1:LP=77343:C=1:N=b'v20190202iAMwho\x00'
UCAST_PI:IP=999.999.85.0:LP=72678:C=1:N=b'v20190202iAMwho\x00'
UCAST_PI:IP=999.999.84.255:LP=61250:C=1:N=b'v20190202iAMwho\x00'
UCAST_PI:IP=999.999.84.252:LP=54943:C=1:N=b'v20190202iAMwho\x00'
PI_REQ:239.199.199.199:1:0/1
SET:UCAST_PI:ClientCnt:6
UCAST_PI:IP=999.999.84.253:LP=57557:C=1:N=b'v20190202iAMwho\x00'
UCAST_PI:IP=999.999.84.254:LP=56130:C=2:N=b'v20190202iAMwho\x00'
UCAST_PI:IP=999.999.85.1:LP=77343:C=2:N=b'v20190202iAMwho\x00'
UCAST_PI:IP=999.999.85.0:LP=72678:C=2:N=b'v20190202iAMwho\x00'
UCAST_PI:IP=999.999.84.255:LP=61250:C=2:N=b'v20190202iAMwho\x00'
UCAST_PI:IP=999.999.84.252:LP=54943:C=2:N=b'v20190202iAMwho\x00'


Summary / Noticed / THoughts
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The mcast conversion to ucast and it eating up most of available bandwidth of
the AP seems to be having a major impact, as expected.

Also if too many clients get associated with a single AP, then there is 70-80%
losses. ANd even if one stops / removes most of the clients from the AP. The
remaining clients will still be impacted by bad performance, till they stop and
rejoin the mcast group.

