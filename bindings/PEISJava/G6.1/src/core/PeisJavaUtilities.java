package core;

import java.util.Date;

final class PeisJavaUtilities {
	
	final static Date getJavaDateFromPeisDate(int s, int us) {

		if(s < 0 || us < 0) {
			return null; //Wildcard (-1) / invalid time stamp
		}
		
		long ms = s;
		ms *= 1000;
		ms += us / 1000;
		
		//This seems to work. The assumption is that the 
		//PEIS time is in UTC and that Java does the necessary adjustments.
		final Date date = new Date(ms);
		System.out.println(date);
		return date;
	}
	
	final static Date setPeisDateFromJavaDate(Date date, int[] out) {
		
		assert out.length == 2;
		final Date oldDate = getJavaDateFromPeisDate(out[0], out[1]);
		
		final long ms = date.getTime();
		final long s = ms / 1000;
		final long us = (ms % 1000)*1000;
		
		out[0] = (int) s;
		out[1] = (int) us;
		
		return oldDate;
	}
	
}
