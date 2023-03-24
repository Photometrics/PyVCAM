Usage
=====

Saving an Image to Disk
-----------------------

To capture an image, the :meth:`get_frame() <pyvcam.driver.PyVCAM.get_frame>` function returns a 2D Numpy array.
To convert the returned array to a visible image, additional tools are required, one of which is shown as an example below.

Using the Pillow library
^^^^^^^^^^^^^^^^^^^^^^^^

One method of saving an image to disk uses the ``Image`` class from ``[pillow](https://python-pillow.org/)``.

.. code-block:: python

    from PIL import Image

After setting up your experiment environment with a PyVCAM device initialized, 
the simplest way to capture an image is to use the :meth:`get_frame() <pyvcam.driver.PyVCAM.get_frame>` function.

The 2D Numpy array returned by the :meth:`get_frame() <pyvcam.driver.PyVCAM.get_frame>` function can be translated to 
a ``.png`` image with the ``Image`` class.

.. code-block:: python
    
    frame = self.pyvcam.get_frame(exp_time=1)
    im = Image.fromarray(frame)
    im.save('/path/to/folder/imagename.png')

The function :meth:`get_sequence() <pyvcam.driver.PyVCAM.get_sequence>` can be used for a rapid succession of image captures,
which returns a 3D Numpy array. The above code can be modified to save selected frames.

For more functions available in the ``Image`` class, refer to the `Pillow Tutorial <https://pillow.readthedocs.io/en/stable/handbook/tutorial.html>`_

**NOTE:** The :meth:`start_live() <pyvcam.driver.PyVCAM.start_live>` function has a parameter that allows for direct saving
to disk, but due to the large file size generated, the function only allows saving as a ``.bin`` file, which requires additional
processing to convert into visible images.