package temporary;
import java.awt.Color;

import javax.swing.ImageIcon;
import javax.swing.JFrame;
import javax.swing.JLabel;
import javax.swing.JPanel;


public class ShowImageFrame extends JFrame
{
	/**
	 * 
	 */
	private static final long serialVersionUID = 5524840483231780599L;
	private ImageIcon icon = null;
	public void setImage(byte[] data) {
		byte[] newData = new byte[data.length-11];
		for (int i = 0; i < data.length-11; i++) newData[i] = data[i+11];
		icon = new ImageIcon(newData);
	}
	public ShowImageFrame(byte[] data) {
		byte[] newData = new byte[data.length-11];
		for (int i = 0; i < data.length-11; i++) newData[i] = data[i+11];
		icon = new ImageIcon(newData);
	}
	public void showImage() {
		setSize(icon.getIconWidth()+50,icon.getIconHeight()+50);
		JPanel panel = new JPanel();
		panel.setBackground(Color.white);
		JLabel label = new JLabel();
		label.setIcon(icon);
		panel.add(label);
		this.getContentPane().add(panel);
		setVisible(true);
	}
} 