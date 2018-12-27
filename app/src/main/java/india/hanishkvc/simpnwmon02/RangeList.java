package india.hanishkvc.simpnwmon02;

/*
    RangeList - Helps maintain a sorted list of ranges
    v20181227IST1156
    HanishKVC, GPL, 2018
 */

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

    public void addRange(int start, int end) {
        for(int i = 0; i < list.size(); i++) {
            if (list.get(i).start > start) {
                if (list.get(i).start > end) {
                    list.add(i, new Range(start, end));
                    break;
                } else {
                    list.get(i).start = start;
                    if (list.get(i).end < end) {
                        list.get(i).end = end;
                    } // else { list.get(i).end = list.get(i).end}
                    break;
                }
                //break
            } else  {
                if (list.get(i).start == start) {
                    if (list.get(i).end < end) {
                        list.get(i).end = end;
                    }
                    break;
                } else {
                    if (list.get(i).end >= end) {
                        break;
                    } else {
                        list.get(i).end = end;
                        break;
                    }
                }
            }
        }
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

}
