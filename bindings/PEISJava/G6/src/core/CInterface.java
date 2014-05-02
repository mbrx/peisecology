package core;

import com.sun.jna.Library; 
import com.sun.jna.Pointer;

public interface CInterface extends Library 
{ 
      public int fflush(Pointer fd);
}  