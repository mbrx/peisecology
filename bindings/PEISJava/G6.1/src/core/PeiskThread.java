package core;

import java.util.HashMap;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;

import core.PeisJavaInterface.PeisCallbackHandle;
import core.PeisJavaInterface.PeisTupleCallback;


class PeiskThread extends Thread {
	
	private final static ExecutorService executorService = Executors.newSingleThreadExecutor();
	private HashMap<PeisTupleCallback, PeisCallbackHandle> callbacks = new HashMap<PeisTupleCallback, PeisCallbackHandle>();
	
	private final static int FREQUENCY_HZ = 100;
	
	public PeiskThread() {
		//This thread should not live by itself
		setDaemon(true);
	}
	
	private class Callback {
		public final int owner;
		public final String key;
		public final PeisTupleCallback tupleCallback;
		
		public Callback(int o, String k, PeisTupleCallback tcb) {
			owner = o;
			key = k;
			tupleCallback = tcb;
		}
		
		@Override
		public String toString() {
			return ("Callback <" + key + "," + owner + "," + tupleCallback.getClass().getName() + ">");
		}
	}
	
	protected PeisTupleCallback registerCallback(String key, int owner, PeisTupleCallback coi) {
		
		if(coi == null) {
			throw new IllegalArgumentException("PeisTupleCallback is null");
		}
		
		final Callback cb = new Callback(owner,key,coi);
		
		executorService.execute(new Runnable() {
			@Override
			public void run() {
				synchronized (PeisJavaMT.peiskCriticalSection) {
					PeisCallbackHandle hndl = PeisJavaMT.INSTANCE.peisk_registerTupleCallback(cb.owner, cb.key, null, cb.tupleCallback);
					System.out.println("REGISTERED (M) " + cb);
					callbacks.put(cb.tupleCallback, hndl);
				}
			}
		});
		
		return coi;
	}
	
	protected PeisTupleCallback registerDeletedCallback(String key, int owner, PeisTupleCallback coi) {
		
		if(coi == null) {
			throw new IllegalArgumentException("PeisTupleCallback is null");
		}
		
		final Callback cb = new Callback(owner,key,coi);
		
		executorService.execute(new Runnable() {
			@Override
			public void run() {
				synchronized (PeisJavaMT.peiskCriticalSection) {
					PeisCallbackHandle hndl = PeisJavaMT.INSTANCE.peisk_registerTupleDeletedCallback(cb.owner, cb.key, null, cb.tupleCallback);
					System.out.println("REGISTERED (D) " + cb);
					callbacks.put(cb.tupleCallback,hndl);
				}
			}
		});
		
		return coi;
	}
	
	protected void unregisterCallback(final PeisTupleCallback tupleCallback) {
		
		if(tupleCallback == null) {
			throw new IllegalArgumentException("PeisTupleCallback is null");
		}
		
		executorService.execute(new Runnable() {
			@Override
			public void run() {
				synchronized (PeisJavaMT.peiskCriticalSection) {
					PeisCallbackHandle hndl = callbacks.get(tupleCallback);
					int ret = PeisJavaMT.INSTANCE.peisk_unregisterTupleCallback(hndl);
					if (ret != 0) {
						try {
							throw new PeisTupleException(ret, PeisJavaMT.INSTANCE);
						} catch (PeisTupleException e) {
							e.printStackTrace();
							System.exit(ret);
						}
					}
				}
			}
		});
	}
	
	@Override
	public void run() {
		
		while(!Thread.interrupted()) {
			
			//Wait one PEIS cycle
//			PeisJavaMT.INSTANCE.peisk_waitOneCycle(1000000 / FREQUENCY_HZ);
			try {
				Thread.sleep(1000 / FREQUENCY_HZ);
			} catch (InterruptedException e) {
				// TODO Auto-generated catch block
				//e.printStackTrace();
				return;
			}
			
			//Step the PEIS kernel
			synchronized(PeisJavaMT.peiskCriticalSection) {
				if(PeisJavaMT.INSTANCE.peisk_isRunning()) {
					PeisJavaMT.INSTANCE.peisk_step();
				} else {
					break;
				}
			}
		}
		
		//Process all waiting tasks
		try {
			executorService.awaitTermination(5, TimeUnit.SECONDS);
		} catch (InterruptedException e) {
			e.printStackTrace();
		}
	}
	
}

