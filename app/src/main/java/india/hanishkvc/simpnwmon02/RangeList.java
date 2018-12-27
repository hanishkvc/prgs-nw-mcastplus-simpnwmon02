package india.hanishkvc.simpnwmon02;

/*
    RangeList - Helps maintain a sorted list of ranges
    v20181227IST1156
    HanishKVC, GPL, 2018
 */

import java.io.FileWriter;
import java.io.IOException;
import java.util.LinkedList;
import java.util.TreeSet;

public class RangeList {
    //TreeSet<Range> set = null;
    LinkedList<Range> list = null;

    private class Range {
        int start;
        int end;

        public Range(int start, int end) {
            this.start = start;
            this.end = end;
        }
    }

    public RangeList() {
        //set = new TreeSet<Range>();
        list = new LinkedList<Range>();
    }

    public void addFromFile(String sFile) {

    }

    public void saveToFile(String sFile) throws IOException {
        FileWriter fw = null;
        try {
            fw = new FileWriter(sFile);
            for (int i = 0; i < list.size(); i++) {
                fw.write(list.get(i).start+"-"+list.get(i).end+"\n");
            }
            fw.close();
        } catch (IOException e) {
            throw new IOException("RL:saveToFile:IOException:"+e.toString());
        }
    }

    public void addRange(int start, int end) {
        for(int i = 0; i < list.size(); i++) {
            if (list.get(i).start > start) {
                if (list.get(i).start > end) {
                    list.add(i, new Range(start, end));
                    return;
                } else {
                    list.get(i).start = start;
                    if (list.get(i).end < end) {
                        list.get(i).end = end;
                    } // else { list.get(i).end = list.get(i).end}
                    return;
                }
                //break
            } else  {
                if (list.get(i).start == start) {
                    if (list.get(i).end < end) {
                        list.get(i).end = end;
                    }
                    return;
                } else {
                    if (list.get(i).end >= end) {
                        return;
                    } else {
                        if (list.get(i).end >= start) {
                            list.get(i).end = end;
                            return;
                        }
                    }
                }
            }
        }
        list.add(new Range(start, end));
    }

    public void remove(int val) {
        for (int i = 0; i < list.size(); i++) {
            if (list.get(i).start < val) {
                if (list.get(i).end > val) {
                    int origEnd = list.get(i).end;
                    list.get(i).end = val-1;
                    list.add(i+1, new Range(val+1, origEnd));
                    break;
                } else {
                    if (list.get(i).end == val) {
                        list.get(i).end = val-1;
                        break;
                    } // else continue checking next ranges in list, by iterating further
                }
            } else {
                if (list.get(i).start == val) {
                    if (list.get(i).end == val) {
                        list.remove(i);
                        break;
                    } else {
                        list.get(i).start = val+1;
                        break;
                    }
                } else {
                    // Nothing to remove from list, as val not in list
                    break;
                }
            }
        }
    }

    public int getStart(int index) {
        return list.get(index).start;
    }

    public int getEnd(int index) {
        return list.get(index).end;
    }

}
