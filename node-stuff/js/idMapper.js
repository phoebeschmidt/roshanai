var relayAddressRegExp = new RegExp("^[1-8]{3}$");
var serverAddress = "http://localhost:3000/solenoids/"

$(document).ready(function() {
  setButtonClickEvents();

});

function setButtonClickEvents() {
  $('.solenoid-go-button').click(function() {
    console.log("CLICKED")
    var solenoid = $(this).parent().parent().parent();
    var id = solenoid.attr("solenoid-id");
    var val = solenoid.find("input").val();
    if (relayAddressRegExp.test(val)) {
      selectsolenoidLogo(id);
      solenoid.find(".error-message").hide();
      mapAddress(id, val);
    } else {
      solenoid.find(".error-message").show();
    }
  });

  $('.solenoid-heart-button').click(function() {
    var solenoid = $(this).parent().parent().parent();
    var id = solenoid.attr('solenoid-id');
    selectsolenoidLogo(id);
  });

}
function selectsolenoidLogo(id) {
  var selectorStr = ".small-img#" + id;
  $(selectorStr).css("color", "red");
  setTimeout(function() {
    $(selectorStr).css("color", "black");
  }, 500);
}

function mapAddress(id, address) {
  var data = {
    address: address
  }
  $.ajax({
    type: "POST",
    url: serverAddress + id + '/address',
    data: JSON.stringify(data),
    success: function(data) {
      console.log(data);
      return
    },
    contentType: 'application/json'
  });
}
