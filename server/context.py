#
# context - a logic to load program related context from a given text file
# v20190111IST1031
# HanishKVC, GPL, 19XY
#
import io
import enum


gbSkipEmptyLines=True



def open(sfContext, sMode="r"):
	fContext = io.open(sfContext, sMode)
	return fContext


LCMode=enum.Enum("lcMode", "NONE, CLIENTS")
def load_clients(fContext):
	fContext.seek(0)
	lcMode = LCMode.NONE
	clients = []
	while True:
		l = fContext.readline()
		if (l == ""):
			break
		l = l.strip()
		if (gbSkipEmptyLines):
			if (l == ""):
				continue
		if (l.startswith("<clients>")):
			if (lcMode != LCMode.NONE):
				print("ERROR:load_clients: logic state wrong at clients_begin")
				break
			lcMode = LCMode.CLIENTS
		elif (l.startswith("</clients>")):
			if (lcMode != LCMode.CLIENTS):
				print("ERROR:load_clients: logic state wrong at clients_end")
				break
			lcMode = LCMode.NONE
			break
		else:
			if (lcMode == LCMode.CLIENTS):
				clients.append(l)
	return clients


def close(fContext):
	fContext.close()

