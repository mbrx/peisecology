package examples;

import java.util.ArrayList;
import java.util.logging.Level;
import java.util.logging.Logger;

import core.WrongPeisKernelVersionException;
import static core.PeisJavaMT.*;

/**
 * This is an example that shows how PEISJava can be used with threads.
 * The example spawns threads that increments counter tuples.
 * @author Jonas Ullberg
 */
public class CounterExample {
	
	private final static Logger logger = Logger.getLogger(CounterExample.class.getPackage().getName());
	
	private final static class TimerThread extends Thread {
		
		protected final String timerName;
		protected final int sleepTime;
		protected volatile boolean doQuit = false;
		
		public TimerThread(String timerName, int sleepTime) {
			this.timerName = timerName;
			this.sleepTime = sleepTime;
		}
		
		@Override
		public void run() {
			
			for(int i = 0; doQuit == false; ++i) {
				final String tupleValue = Integer.toString(i);
				
				peisjava_setStringTuple(timerName, tupleValue);
				logger.log(Level.INFO, "Setting " + timerName + " to " + tupleValue);
				
				try {
					Thread.sleep(sleepTime);
				} catch (InterruptedException e) {
					e.printStackTrace();
				}
			}
		}
		
		protected void doQuit() {
			doQuit = true;
			
			while(this.isAlive()) {
				try {
					Thread.sleep(100);
				} catch (InterruptedException e) {
					e.printStackTrace();
				}
			}
		}
	}
	
	private final static String DO_QUIT_TUPLE_NAME = "do-quit";
	
	public static void main(String[] args) {
		
		try {
			peisjava_initialize(args);
		} catch (WrongPeisKernelVersionException e) {
			e.printStackTrace();
			System.exit(1);
		}
		
		ArrayList<TimerThread> timerThreads = new ArrayList<TimerThread>();
		
		timerThreads.add(new TimerThread("timer_a", 250));
		timerThreads.add(new TimerThread("timer_b", 500));
		timerThreads.add(new TimerThread("timer_c", 1000));
		timerThreads.add(new TimerThread("timer_d", 2000));
		timerThreads.add(new TimerThread("timer_e", 4000));
		
		for (TimerThread timerThread : timerThreads) {
			timerThread.start();
		}

		peisjava_setStringTuple(DO_QUIT_TUPLE_NAME, "");
		for(;;) {
			final String quit = peisjava_getStringTuple(DO_QUIT_TUPLE_NAME);
			if(quit != null && quit.length() > 0) {
				break;
			}
			
			try {
				Thread.sleep(1000);
			} catch (InterruptedException e) {
				e.printStackTrace();
			}
		}
		
		for (TimerThread timerThread : timerThreads) {
			timerThread.doQuit();
		}
		
		peisjava_shutdown();
		System.exit(0);
	}
}
