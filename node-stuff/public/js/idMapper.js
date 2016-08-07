var relayAddressRegExp = new RegExp("^0[1-8]{2}$");
var serverAddress = "http://localhost:3000/solenoids/"

$(document).ready(function() {
  setButtonClickEvents();
  getInitialAddresses();
});

function setButtonClickEvents() {
  $('.solenoid-go-button').click(function() {
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

function getInitialAddresses() {
  $.ajax({
    type: "GET",
    url: "/solenoids/addresses",
    success: function(data) {
      writeAddressesToHearts(data);
      writeAddressesToInputs(data);
    }
  });
}

function writeAddressesToHearts(data) {
  for (var id in data) {
    $('.small-img#' + id).html('').append(data[id]);
  }
}

function writeAddressToHeart(id, data) {
  $('.small-img#' + id).html('').append(data);
}

function writeAddressesToInputs(data) {
  for (var id in data) {
    $('.solenoid[solenoid-id=' + id +'] input').val(data[id]);
  }
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
    url:  '/solenoids/' + id + '/address',
    data: JSON.stringify(data),
    success: function(data) {
      writeAddressToHeart(id, address);
      return
    },
    contentType: 'application/json'
  });
}
